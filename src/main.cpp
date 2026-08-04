#include <QApplication>
#include <QColorDialog>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGraphicsOpacityEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMap>
#include <QMouseEvent>
#include <QProcess>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QRegion>
#include <QScreen>
#include <QSettings>
#include <QShowEvent>
#include <QSlider>
#include <QSpinBox>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <LayerShellQt/Window>
#include <functional>

class DragBar final : public QLabel {
public:
    explicit DragBar(QWidget *window, std::function<void(QPoint, bool)> dragWindow,
                     bool systemMove = false)
        : QLabel("•••", window), dragWindow_(std::move(dragWindow)), systemMove_(systemMove) {
        setObjectName("dragBar");
        setAlignment(Qt::AlignCenter);
        setFixedHeight(14);
        setCursor(Qt::SizeAllCursor);
        setFocusPolicy(Qt::NoFocus);
    }

protected:
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            dragging_ = true;
            if (!systemMove_)
                grabMouse();
            dragWindow_(event->globalPosition().toPoint(), true);
            event->accept();
        }
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        if (dragging_ && (event->buttons() & Qt::LeftButton)) {
            dragWindow_(event->globalPosition().toPoint(), false);
            event->accept();
        }
    }

    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            if (dragging_)
                dragWindow_(event->globalPosition().toPoint(), false);
            dragging_ = false;
            if (!systemMove_)
                releaseMouse();
            event->accept();
        }
    }

private:
    std::function<void(QPoint, bool)> dragWindow_;
    bool systemMove_ = false;
    bool dragging_ = false;
};

class ResizeGrip final : public QWidget {
public:
    explicit ResizeGrip(QWidget *parent, std::function<void(QPoint)> resizePanel)
        : QWidget(parent), resizePanel_(std::move(resizePanel)) {
        setFixedSize(18, 18);
        setCursor(Qt::SizeFDiagCursor);
        setStyleSheet("background: transparent;");
    }

protected:
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            start_ = event->globalPosition().toPoint();
            startSize_ = parentWidget()->size();
            dragging_ = true;
            event->accept();
        }
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        if (dragging_ && (event->buttons() & Qt::LeftButton)) {
            resizePanel_(QPoint(startSize_.width(), startSize_.height()) +
                         (event->globalPosition().toPoint() - start_));
            event->accept();
        }
    }

    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            dragging_ = false;
            event->accept();
        }
    }

private:
    std::function<void(QPoint)> resizePanel_;
    QPoint start_;
    QSize startSize_;
    bool dragging_ = false;
};

class KeyButton final : public QPushButton {
public:
    explicit KeyButton(const QString &label, QWidget *parent = nullptr)
        : QPushButton(label, parent) {}

    void setRightClickAction(std::function<void()> action) {
        rightClickAction_ = std::move(action);
    }

    void setModifierKey(bool modifier, bool active, bool locked) {
        modifier_ = modifier;
        setProperty("modifier", modifier);
        setProperty("activeModifier", active);
        setProperty("lockedModifier", locked);
        refreshStyle();
    }

    void flashText(const QString &text) {
        const QString original = this->text();
        const int flash = ++flashGeneration_;
        setText(text);
        QTimer::singleShot(180, this, [this, original, flash] {
            if (flash == flashGeneration_)
                setText(original);
        });
    }

protected:
    void mousePressEvent(QMouseEvent *event) override {
        if (modifier_ && (event->button() == Qt::LeftButton || event->button() == Qt::RightButton)) {
            setProperty("modifierPressed", true);
            refreshStyle();
        }
        QPushButton::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override {
        if (modifier_) {
            QTimer::singleShot(220, this, [this] {
                setProperty("modifierPressed", false);
                refreshStyle();
            });
        }
        if (event->button() == Qt::RightButton && rect().contains(event->position().toPoint())) {
            if (rightClickAction_)
                rightClickAction_();
            event->accept();
            return;
        }
        QPushButton::mouseReleaseEvent(event);
    }

private:
    void refreshStyle() {
        style()->unpolish(this);
        style()->polish(this);
        update();
    }

    std::function<void()> rightClickAction_;
    bool modifier_ = false;
    int flashGeneration_ = 0;
};

class Keyboard final : public QWidget {
public:
    Keyboard() {
        setWindowTitle("Orbit Keyboard");
        setObjectName("keyboard");
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                       Qt::WindowDoesNotAcceptFocus);
        setAttribute(Qt::WA_ShowWithoutActivating, true);
        setAttribute(Qt::WA_X11DoNotAcceptFocus, true);
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_TranslucentBackground, true);
        settings_.beginGroup("appearance");
        repeatDelay_ = settings_.value("repeatDelay", 450).toInt();
        repeatInterval_ = settings_.value("repeatInterval", 55).toInt();
        keySpacing_ = settings_.value("keySpacing", 2).toInt();
        borderWidth_ = settings_.value("borderWidth", 1).toInt();
        opacity_ = settings_.value("opacity", 100).toInt();
        keyColor_ = settings_.value("keyColor", "#202c3d").toString();
        borderColor_ = settings_.value("borderColor", "#101722").toString();
        settings_.endGroup();

        panel_ = new QWidget(this);
        panel_->setObjectName("keyboardPanel");
        panel_->setAttribute(Qt::WA_StyledBackground, true);
        panel_->setMinimumSize(420, 150);
        panel_->resize(580 + keySpacing_, 199);
        settings_.beginGroup("window");
        const int savedWidth = settings_.value("width", panel_->width()).toInt();
        const int savedHeight = settings_.value("height", panel_->height()).toInt();
        savedPanelPosition_ = QPoint(settings_.value("x", 0).toInt(),
                                     settings_.value("y", 0).toInt());
        restorePanelPosition_ = settings_.contains("x") && settings_.contains("y");
        settings_.endGroup();
        panel_->resize(qMax(panel_->minimumWidth(), savedWidth),
                       qMax(panel_->minimumHeight(), savedHeight));
        opacityEffect_ = new QGraphicsOpacityEffect(panel_);
        panel_->setGraphicsEffect(opacityEffect_);

        auto *panelLayout = new QVBoxLayout(panel_);
        panelLayout->setContentsMargins(0, 0, 0, 0);
        panelLayout->setSpacing(1);

        auto *topBar = new QHBoxLayout;
        topBar->setContentsMargins(0, 0, 0, 0);
        topBar->setSpacing(1);
        topBar->addWidget(new DragBar(panel_, [this](QPoint pointer, bool begin) {
            dragTo(pointer, begin);
        }), 1);
        auto *monitorButton = new QPushButton("⇄", panel_);
        monitorButton->setObjectName("monitorButton");
        monitorButton->setFocusPolicy(Qt::NoFocus);
        monitorButton->setFixedSize(22, 14);
        monitorButton->setToolTip("Move keyboard to next monitor");
        connect(monitorButton, &QPushButton::clicked, this, [this] {
            const auto screens = QGuiApplication::screens();
            if (screens.size() < 2 || !layer_)
                return;
            QScreen *current = windowHandle()->screen();
            int index = screens.indexOf(current);
            QScreen *next = screens.at((index + 1) % screens.size());
            hide();
            windowHandle()->setScreen(next);
            layer_->setWantsToBeOnActiveScreen(false);
            layer_->setScreen(next);
            QTimer::singleShot(50, this, [this] {
                show();
                raise();
            });
            QTimer::singleShot(200, this, [this] {
                movePanel(QPoint((width() - panel_->width()) / 2,
                                 height() - panel_->height()));
            });
        });
        topBar->addWidget(monitorButton);
        auto *closeButton = new QPushButton("×", panel_);
        closeButton->setObjectName("closeButton");
        closeButton->setFocusPolicy(Qt::NoFocus);
        closeButton->setFixedSize(22, 14);
        closeButton->setToolTip("Close keyboard");
        connect(closeButton, &QPushButton::clicked, this, &QWidget::close);
        topBar->addWidget(closeButton);
        panelLayout->addLayout(topBar);

        body_ = new QHBoxLayout;
        body_->setContentsMargins(0, 0, 0, 0);
        body_->setSpacing(keySpacing_);

        auto *mainKeys = new QWidget(panel_);
        mainKeys->setMinimumWidth(0);
        keys_ = new QVBoxLayout(mainKeys);
        keys_->setContentsMargins(2, 0, 0, 2);
        keys_->setSpacing(2);
        body_->addWidget(mainKeys);

        auto *sidePanel = new QWidget(panel_);
        sidePanel->setFixedWidth(48);
        sideKeys_ = new QVBoxLayout(sidePanel);
        sideKeys_->setContentsMargins(0, 0, 0, 2);
        sideKeys_->setSpacing(keySpacing_);
        auto addSideKey = [this](const QString &label, const QString &action,
                                 const QString &tooltip) {
            auto *button = key(label, action);
            if (action == "CUT" || action == "COPY" || action == "PASTE")
                button->setProperty("clipboardAction", action);
            button->setToolTip(tooltip);
            sideKeys_->addWidget(button, 1);
        };
        addSideKey("Del", "DELETE", "Delete");
        addSideKey("✂", "CUT", "Cut (Ctrl+X)");
        addSideKey("⧉", "COPY", "Copy (Ctrl+C)");
        addSideKey("▤", "PASTE", "Paste (Ctrl+V)");
        addSideKey("|←", "HOME", "Home");
        addSideKey("→|", "END", "End");
        body_->addWidget(sidePanel);
        panelLayout->addLayout(body_);

        resizeGrip_ = new ResizeGrip(panel_, [this](QPoint delta) {
            panel_->resize(qMax(panel_->minimumWidth(), delta.x()),
                           qMax(panel_->minimumHeight(), delta.y()));
            panel_->layout()->activate();
            resizeGrip_->move(panel_->width() - resizeGrip_->width(),
                              panel_->height() - resizeGrip_->height());
            movePanel(panel_->pos());
        });
        resizeGrip_->move(panel_->width() - resizeGrip_->width(),
                          panel_->height() - resizeGrip_->height());

        buildKeys();
        applyStyle();

        // QWidget creates its backing QWindow lazily. Materialize it before
        // attaching the layer-shell role.
        winId();
        layer_ = LayerShellQt::Window::get(windowHandle());
        layer_->setLayer(LayerShellQt::Window::LayerOverlay);
        layer_->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
        layer_->setActivateOnShow(false);
        LayerShellQt::Window::Anchors anchors;
        anchors.setFlag(LayerShellQt::Window::AnchorTop);
        anchors.setFlag(LayerShellQt::Window::AnchorLeft);
        anchors.setFlag(LayerShellQt::Window::AnchorRight);
        anchors.setFlag(LayerShellQt::Window::AnchorBottom);
        layer_->setAnchors(anchors);
        layer_->setExclusiveZone(0);
        layer_->setDesiredSize(QSize(0, 0));
    }

protected:
    void showEvent(QShowEvent *event) override {
        QWidget::showEvent(event);
        QTimer::singleShot(150, this, [this] {
            if (restorePanelPosition_)
                movePanel(savedPanelPosition_);
            else
                movePanel(QPoint((width() - panel_->width()) / 2,
                                 height() - panel_->height()));
        });
    }

    void closeEvent(QCloseEvent *event) override {
        settings_.beginGroup("window");
        settings_.setValue("width", panel_->width());
        settings_.setValue("height", panel_->height());
        settings_.setValue("x", panel_->x());
        settings_.setValue("y", panel_->y());
        settings_.endGroup();
        settings_.sync();
        QWidget::closeEvent(event);
    }

    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        if (resizeGrip_)
            resizeGrip_->move(panel_->width() - resizeGrip_->width(),
                              panel_->height() - resizeGrip_->height());
        if (panel_)
            setMask(QRegion(panel_->geometry()));
    }

private:
    enum class ModifierState { Off, Armed, Locked };

    QVBoxLayout *keys_{};
    QVBoxLayout *sideKeys_{};
    QHBoxLayout *body_{};
    QWidget *panel_{};
    ResizeGrip *resizeGrip_{};
    QGraphicsOpacityEffect *opacityEffect_{};
    LayerShellQt::Window *layer_{};
    QSettings settings_;
    int repeatDelay_ = 450;
    int repeatInterval_ = 55;
    int keySpacing_ = 2;
    int borderWidth_ = 1;
    int opacity_ = 100;
    QString keyColor_ = "#202c3d";
    QString borderColor_ = "#101722";
    ModifierState shift_ = ModifierState::Off;
    ModifierState ctrl_ = ModifierState::Off;
    ModifierState alt_ = ModifierState::Off;
    ModifierState altGr_ = ModifierState::Off;
    ModifierState meta_ = ModifierState::Off;
    bool capsLock_ = false;
    QPoint dragPointerStart_;
    QPoint dragPanelStart_;
    QPoint savedPanelPosition_;
    bool restorePanelPosition_ = false;

    void movePanel(QPoint position) {
        position.setX(qBound(0, position.x(), qMax(0, width() - panel_->width())));
        position.setY(qBound(0, position.y(), qMax(0, height() - panel_->height())));
        panel_->move(position);
        setMask(QRegion(panel_->geometry()));
        settings_.beginGroup("window");
        settings_.setValue("x", panel_->x());
        settings_.setValue("y", panel_->y());
        settings_.endGroup();
    }

    void dragTo(const QPoint &pointer, bool begin) {
        if (begin) {
            dragPointerStart_ = pointer;
            dragPanelStart_ = panel_->pos();
            return;
        }
        if (layer_) {
            if (QScreen *screen = QGuiApplication::screenAt(pointer);
                screen && screen != windowHandle()->screen()) {
                windowHandle()->setScreen(screen);
                layer_->setWantsToBeOnActiveScreen(true);
                QTimer::singleShot(0, this, [this] {
                    movePanel(QPoint((width() - panel_->width()) / 2,
                                     height() - panel_->height()));
                });
                dragPointerStart_ = pointer;
                dragPanelStart_ = panel_->pos();
                return;
            }
        }
        const QPoint delta = pointer - dragPointerStart_;
        movePanel(dragPanelStart_ + delta);
    }

    QPushButton *key(const QString &label, const QString &action = {}, int width = 1,
                     const QString &inputLabel = {}) {
        auto *button = new KeyButton(label);
        button->setFocusPolicy(Qt::NoFocus);
        button->setAttribute(Qt::WA_MacShowFocusRect, false);
        button->setProperty("special", !action.isEmpty());
        const bool modifier = action == "SHIFT" || action == "CTRL" ||
                              action == "ALT" || action == "RIGHTALT" ||
                              action == "META" || action == "CAPSLOCK";
        button->setModifierKey(modifier, modifierActive(action), modifierLocked(action));
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        const bool repeatable = action.isEmpty() || action == "SPACE" || action == "BACKSPACE" ||
                                action == "DELETE" || action == "RETURN" ||
                                action == "LEFT" || action == "DOWN" ||
                                action == "UP" || action == "RIGHT";
        button->setAutoRepeat(repeatable);
        button->setAutoRepeatDelay(repeatDelay_);
        button->setAutoRepeatInterval(repeatInterval_);

        if (action == "SHIFT") {
            connect(button, &QPushButton::clicked, this, [this] {
                advanceModifier(shift_);
                buildKeys();
            });
        } else if (action == "CAPSLOCK") {
            connect(button, &QPushButton::clicked, this, [this] {
                pressKey("CAPSLOCK");
                capsLock_ = !capsLock_;
                buildKeys();
            });
        } else if (action == "CTRL" || action == "ALT" ||
                   action == "RIGHTALT" || action == "META") {
            connect(button, &QPushButton::clicked, this, [this, action] {
                toggleModifier(action);
                buildKeys();
            });
        } else if (action == "OPTIONS") {
            connect(button, &QPushButton::clicked, this, [this, button] {
                button->setEnabled(false);
                showOptions(button);
            });
        } else if (action == "INSERT") {
            connect(button, &QPushButton::clicked, this, [this, button] {
                button->setEnabled(false);
                showInsertDialog();
                button->setEnabled(true);
            });
        } else if (action == "CUT" || action == "COPY" || action == "PASTE") {
            connect(button, &QPushButton::clicked, this, [this, action] {
                pressClipboardShortcut(action);
            });
        } else if (!action.isEmpty()) {
            connect(button, &QPushButton::clicked, this, [this, action] { pressKey(action); });
        } else {
            const QString input = inputLabel.isEmpty() ? label : inputLabel;
            connect(button, &QPushButton::clicked, this, [this, input] {
                typePrintable(input, modifierActive("SHIFT"));
            });
        }
        if (action == "RETURN") {
            button->setRightClickAction([this] { pressShiftEnter(); });
        } else if (action.isEmpty()) {
            const QString input = inputLabel.isEmpty() ? label : inputLabel;
            button->setRightClickAction([this, button, input] {
                button->flashText(shiftedText(input));
                typePrintable(input, true);
            });
        }
        button->setProperty("keyWidth", width);
        return button;
    }

    void clearKeys() {
        while (auto *item = keys_->takeAt(0)) {
            if (auto *row = item->layout())
                while (auto *child = row->takeAt(0))
                    delete child->widget();
            delete item;
        }
    }

    void addRow(const QStringList &labels, const QStringList &actions,
                const QList<int> &widths, const QStringList &inputLabels = {}) {
        auto *row = new QHBoxLayout;
        row->setSpacing(keySpacing_);
        row->setContentsMargins(0, 0, 0, 0);
        for (int i = 0; i < labels.size(); ++i) {
            const QString action = i < actions.size() ? actions[i] : QString();
            const int width = i < widths.size() ? widths[i] : 1;
            const QString input = i < inputLabels.size() ? inputLabels[i] : QString();
            row->addWidget(key(labels[i], action, width, input), width);
        }
        keys_->addLayout(row, 1);
    }

    QString displayPrintable(const QString &label) const {
        return shiftActive() ? shiftedText(label) : label;
    }

    QStringList displayPrintables(const QStringList &labels) const {
        QStringList displayed;
        for (const QString &label : labels)
            displayed << (label.isEmpty() ? label : displayPrintable(label));
        return displayed;
    }

    void buildKeys() {
        clearKeys();
        keys_->setSpacing(keySpacing_);
        if (sideKeys_)
            sideKeys_->setSpacing(keySpacing_);
        if (body_)
            body_->setSpacing(keySpacing_);
        addRow({"Esc","F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12"},
               {"ESC","F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12"},
               {});
        const auto numberRow = QStringList{"`","1","2","3","4","5","6","7","8","9","0","-","=","⌫"};
        addRow(displayPrintables(numberRow),
               {"","","","","","","","","","","","","","BACKSPACE"},
               {1,1,1,1,1,1,1,1,1,1,1,1,1,2}, numberRow);
        const auto topRow = QStringList{"↹","q","w","e","r","t","y","u","i","o","p","[","]","↵"};
        auto insertRow = displayPrintables(topRow);
        insertRow[13] = "Insert";
        addRow(insertRow,
               {"TAB","","","","","","","","","","","","","INSERT"},
               {2,1,1,1,1,1,1,1,1,1,1,1,1,2}, topRow);
        const auto homeRow = QStringList{"⇪","a","s","d","f","g","h","j","k","l",";","'","\\","↵"};
        addRow(displayPrintables(homeRow),
               {"CAPSLOCK","","","","","","","","","","","","","RETURN"},
               {2,1,1,1,1,1,1,1,1,1,1,1,1,2}, homeRow);
        const auto bottomRow = QStringList{"<","z","x","c","v","b","n","m",",",".","/"};
        addRow({modifierActive("SHIFT") ? "⇧•" : "⇧", displayPrintables(bottomRow)[0],
                displayPrintables(bottomRow)[1], displayPrintables(bottomRow)[2],
                displayPrintables(bottomRow)[3], displayPrintables(bottomRow)[4],
                displayPrintables(bottomRow)[5], displayPrintables(bottomRow)[6],
                displayPrintables(bottomRow)[7], displayPrintables(bottomRow)[8],
                displayPrintables(bottomRow)[9], displayPrintables(bottomRow)[10], "⇧"},
               {"SHIFT","","","","","","","","","","","","SHIFT"},
               {2,1,1,1,1,1,1,1,1,1,1,1,2},
               {"SHIFT","<","z","x","c","v","b","n","m",",",".","/","SHIFT"});
        addRow({"Ctrl","Meta","Alt","","Menu","←","↓","↑","→"},
               {"CTRL","META","ALT","SPACE","OPTIONS","LEFT","DOWN","UP","RIGHT"},
               {2,2,2,8,2,1,1,1,1});
    }

    QString shiftedText(const QString &text) const {
        static const QMap<QString, QString> shifted {
            {"`","~"},{"1","!"},{"2","@"},{"3","#"},{"4","$"},{"5","%"},
            {"6","^"},{"7","&"},{"8","*"},{"9","("},{"0",")"},{"-","_"},
            {"=","+"},{"[","{"},{"]","}"},{";",":"},{"'","\""},{"\\","|"},
            {",","<"},{".",">"},{"/","?"},{"<",">"}
        };
        return shifted.value(text, text.toUpper());
    }

    bool modifierActive(const QString &action) const {
        return modifierState(action) != ModifierState::Off;
    }

    bool modifierLocked(const QString &action) const {
        return modifierState(action) == ModifierState::Locked;
    }

    ModifierState modifierState(const QString &action) const {
        if (action == "SHIFT") return shift_;
        if (action == "CTRL") return ctrl_;
        if (action == "ALT") return alt_;
        if (action == "RIGHTALT") return altGr_;
        if (action == "META") return meta_;
        if (action == "CAPSLOCK") return capsLock_ ? ModifierState::Locked : ModifierState::Off;
        return ModifierState::Off;
    }

    void advanceModifier(ModifierState &state) {
        if (state == ModifierState::Off) state = ModifierState::Armed;
        else if (state == ModifierState::Armed) state = ModifierState::Locked;
        else state = ModifierState::Off;
    }

    void clearIfArmed(ModifierState &state) {
        if (state == ModifierState::Armed)
            state = ModifierState::Off;
    }

    void toggleModifier(const QString &action) {
        if (action == "CTRL") advanceModifier(ctrl_);
        else if (action == "ALT") advanceModifier(alt_);
        else if (action == "RIGHTALT") advanceModifier(altGr_);
        else if (action == "META") advanceModifier(meta_);
    }

    void clearModifiers() {
        const bool changed = anyArmedModifier();
        clearIfArmed(shift_);
        clearIfArmed(ctrl_);
        clearIfArmed(alt_);
        clearIfArmed(altGr_);
        clearIfArmed(meta_);
        if (changed)
            buildKeys();
    }

    void clearAllModifiers() {
        shift_ = ctrl_ = alt_ = altGr_ = meta_ = ModifierState::Off;
    }

    bool anyLockedModifier() const {
        return shift_ == ModifierState::Locked || ctrl_ == ModifierState::Locked ||
               alt_ == ModifierState::Locked || altGr_ == ModifierState::Locked ||
               meta_ == ModifierState::Locked;
    }

    bool anyArmedModifier() const {
        return shift_ == ModifierState::Armed || ctrl_ == ModifierState::Armed ||
               alt_ == ModifierState::Armed || altGr_ == ModifierState::Armed ||
               meta_ == ModifierState::Armed;
    }

    bool hasModifier(const ModifierState state) const {
        return state != ModifierState::Off;
    }

    bool shiftActive() const {
        return hasModifier(shift_);
    }

    bool ctrlActive() const {
        return hasModifier(ctrl_);
    }

    bool altActive() const {
        return hasModifier(alt_);
    }

    bool altGrActive() const {
        return hasModifier(altGr_);
    }

    bool metaActive() const {
        return hasModifier(meta_);
    }

    void typePrintable(const QString &label, bool forceShift) {
        static const QMap<QString, QString> codes {
            {"`","41"},{"1","2"},{"2","3"},{"3","4"},{"4","5"},{"5","6"},
            {"6","7"},{"7","8"},{"8","9"},{"9","10"},{"0","11"},{"-","12"},{"=","13"},
            {"q","16"},{"w","17"},{"e","18"},{"r","19"},{"t","20"},{"y","21"},{"u","22"},
            {"i","23"},{"o","24"},{"p","25"},{"[","26"},{"]","27"},
            {"a","30"},{"s","31"},{"d","32"},{"f","33"},{"g","34"},{"h","35"},{"j","36"},
            {"k","37"},{"l","38"},{";","39"},{"'","40"},{"\\","43"},
            {"z","44"},{"x","45"},{"c","46"},{"v","47"},{"b","48"},{"n","49"},{"m","50"},
            {",","51"},{".","52"},{"/","53"},{"<","86"}
        };
        if (codes.contains(label))
            sendChord(codes[label], forceShift || shiftActive());
        else
            typeText(forceShift || shiftActive() ? shiftedText(label) : label);
        clearModifiers();
    }

    void typeText(const QString &text) {
        runYdotool({"type", "--key-delay", "0", "--", text});
    }

    void showInsertDialog() {
        QDialog dialog;
        dialog.setWindowTitle("Insert");
        dialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        dialog.setAttribute(Qt::WA_ShowWithoutActivating, false);
        dialog.setStyleSheet(QString(R"(
            QDialog {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                            stop:0 #172333, stop:0.45 #0f1825, stop:1 #0a1018);
                border: 1px solid #26384f;
                border-radius: 8px;
            }
            QLabel { background: transparent; color: #eef2f7; }
            #dragBar {
                background: %1;
                color: #607087;
                border: 1px solid %2;
                border-radius: 4px;
                font: 9px "Noto Sans";
            }
            QListWidget, QLineEdit {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                            stop:0 %1, stop:1 #182536);
                color: #ffffff;
                border: 1px solid %2;
                border-radius: 4px;
                selection-background-color: #5576a3;
                selection-color: #ffffff;
                padding: 4px;
            }
            QListWidget::item { color: #ffffff; padding: 4px; }
            QListWidget::item:hover { background: #2b3a50; }
            QPushButton {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                            stop:0 %1, stop:1 #182536);
                color: #ffffff;
                border: 1px solid %2;
                border-radius: 4px;
                padding: 5px 10px;
            }
            QPushButton:hover {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                            stop:0 #3a506d, stop:1 #263a54);
                border-color: #6b8db7;
            }
            QPushButton:pressed { background: #3a4c65; }
            QPushButton#closeButton { padding: 0; }
        )").arg(keyColor_, borderColor_));
        dialog.resize(620, 420);
        auto *layout = new QVBoxLayout(&dialog);
        layout->setContentsMargins(4, 4, 4, 4);
        auto *titleBar = new QHBoxLayout;
        titleBar->setContentsMargins(0, 0, 0, 0);
        titleBar->addWidget(new DragBar(&dialog, [&dialog](QPoint, bool begin) {
            if (begin && dialog.windowHandle())
                dialog.windowHandle()->startSystemMove();
        }, true), 1);
        auto *close = new QPushButton("×", &dialog);
        close->setFixedSize(22, 18);
        close->setToolTip("Close Insert window");
        titleBar->addWidget(close);
        layout->addLayout(titleBar);
        QObject::connect(close, &QPushButton::clicked, &dialog, &QDialog::reject);
        auto *columns = new QHBoxLayout;
        auto *snippetsColumn = new QVBoxLayout;
        auto *clipboardColumn = new QVBoxLayout;
        snippetsColumn->addWidget(new QLabel("Snippets", &dialog));
        clipboardColumn->addWidget(new QLabel("Clipboard", &dialog));
        auto *snippetFilter = new QLineEdit(&dialog);
        auto *clipboardFilter = new QLineEdit(&dialog);
        snippetFilter->setPlaceholderText("Filter snippets");
        clipboardFilter->setPlaceholderText("Filter clipboard");
        snippetsColumn->addWidget(snippetFilter);
        clipboardColumn->addWidget(clipboardFilter);
        auto *snippetList = new QListWidget(&dialog);
        auto *clipboardList = new QListWidget(&dialog);
        snippetsColumn->addWidget(snippetList);
        clipboardColumn->addWidget(clipboardList);
        columns->addLayout(snippetsColumn);
        columns->addLayout(clipboardColumn);
        layout->addLayout(columns);

        auto *editor = new QLineEdit(&dialog);
        editor->setPlaceholderText("New snippet text");
        editor->setFocus();
        layout->addWidget(editor);
        auto *buttons = new QHBoxLayout;
        auto *add = new QPushButton("Add", &dialog);
        auto *edit = new QPushButton("Edit", &dialog);
        auto *remove = new QPushButton("Remove", &dialog);
        auto *insert = new QPushButton("Insert", &dialog);
        buttons->addWidget(add);
        buttons->addWidget(edit);
        buttons->addWidget(remove);
        buttons->addStretch();
        buttons->addWidget(insert);
        layout->addLayout(buttons);

        auto *snippetValues = new QStringList;
        settings_.beginGroup("snippets");
        *snippetValues = settings_.value("items").toStringList();
        settings_.endGroup();
        for (const auto &snippet : *snippetValues) {
            auto *item = new QListWidgetItem(snippet.left(80), snippetList);
            item->setToolTip(snippet);
        }

        QProcess klipper;
        klipper.start("qdbus", {"org.kde.klipper", "/klipper",
                                 "org.kde.klipper.klipper.getClipboardHistoryMenu"});
        if (klipper.waitForFinished(1000)) {
            const auto history = QString::fromLocal8Bit(klipper.readAllStandardOutput())
                                     .split('\n', Qt::SkipEmptyParts);
            for (int i = 0; i < history.size(); ++i) {
                // Klipper represents image entries as labels such as "▨ 303x537".
                // They are not text clipboard entries and must not be shown here.
                if (history[i].contains(QRegularExpression(QStringLiteral("(^|\\s)\\d+x\\d+(\\s|$)"))) ||
                    history[i].startsWith(QStringLiteral("▨")))
                    continue;
                QProcess itemQuery;
                itemQuery.start("qdbus", {"org.kde.klipper", "/klipper",
                                           "org.kde.klipper.klipper.getClipboardHistoryItem",
                                           QString::number(i)});
                if (!itemQuery.waitForFinished(1000)) continue;
                const QString value = QString::fromLocal8Bit(itemQuery.readAllStandardOutput()).trimmed();
                if (value.isEmpty()) continue;
                if (value.startsWith(QStringLiteral("▨")) ||
                    value.contains(QRegularExpression(QStringLiteral("^\\d+x\\d+$"))))
                    continue;
                auto *item = new QListWidgetItem(value.left(80), clipboardList);
                item->setData(Qt::UserRole, i);
                item->setToolTip(value);
            }
        }

        auto filterList = [](QLineEdit *filter, QListWidget *list) {
            QObject::connect(filter, &QLineEdit::textChanged, list, [filter, list] {
                const QString query = filter->text().trimmed();
                for (int i = 0; i < list->count(); ++i)
                    list->item(i)->setHidden(!query.isEmpty() &&
                                              !list->item(i)->toolTip().contains(query, Qt::CaseInsensitive));
            });
        };
        filterList(snippetFilter, snippetList);
        filterList(clipboardFilter, clipboardList);

        connect(snippetList, &QListWidget::currentRowChanged, &dialog, [editor, snippetValues](int row) {
            if (row >= 0 && row < snippetValues->size()) editor->setText(snippetValues->at(row));
        });
        connect(add, &QPushButton::clicked, &dialog, [snippetList, editor, snippetValues, this] {
            if (editor->text().isEmpty()) return;
            snippetValues->append(editor->text());
            auto *item = new QListWidgetItem(editor->text().left(80), snippetList);
            item->setToolTip(editor->text());
            settings_.beginGroup("snippets"); settings_.setValue("items", *snippetValues); settings_.endGroup();
        });
        connect(edit, &QPushButton::clicked, &dialog, [snippetList, editor, snippetValues, this] {
            const int row = snippetList->currentRow();
            if (row < 0 || row >= snippetValues->size() || editor->text().isEmpty()) return;
            (*snippetValues)[row] = editor->text();
            snippetList->item(row)->setText(editor->text().left(80));
            snippetList->item(row)->setToolTip(editor->text());
            settings_.beginGroup("snippets"); settings_.setValue("items", *snippetValues); settings_.endGroup();
        });
        connect(remove, &QPushButton::clicked, &dialog, [snippetList, snippetValues, this] {
            const int row = snippetList->currentRow();
            if (row < 0 || row >= snippetValues->size()) return;
            snippetValues->removeAt(row); delete snippetList->takeItem(row);
            settings_.beginGroup("snippets"); settings_.setValue("items", *snippetValues); settings_.endGroup();
        });
        connect(insert, &QPushButton::clicked, &dialog, [&] {
            QString value;
            if (snippetList->currentRow() >= 0)
                value = snippetValues->at(snippetList->currentRow());
            else if (clipboardList->currentRow() >= 0) {
                QProcess item;
                const int historyIndex = clipboardList->currentItem()->data(Qt::UserRole).toInt();
                item.start("qdbus", {"org.kde.klipper", "/klipper", "org.kde.klipper.klipper.getClipboardHistoryItem", QString::number(historyIndex)});
                if (item.waitForFinished(1000)) value = QString::fromLocal8Bit(item.readAllStandardOutput()).trimmed();
            }
            dialog.done(value.isEmpty() ? QDialog::Rejected : QDialog::Accepted);
            if (!value.isEmpty())
                QTimer::singleShot(0, this, [this, value] { typeText(value); });
        });
        connect(snippetList, &QListWidget::itemDoubleClicked, insert,
                [insert](QListWidgetItem *) { insert->click(); });
        connect(clipboardList, &QListWidget::itemDoubleClicked, insert,
                [insert](QListWidgetItem *) { insert->click(); });
        dialog.show();
        dialog.raise();
        dialog.activateWindow();
        dialog.exec();
        delete snippetValues;
    }

    void pressKey(const QString &name) {
        static const QMap<QString, QString> codes {
            {"ESC","1"},{"BACKSPACE","14"},{"TAB","15"},{"RETURN","28"},
            {"CTRL","29"},{"SHIFT","42"},{"ALT","56"},{"CAPSLOCK","58"},
            {"F1","59"},{"F2","60"},{"F3","61"},{"F4","62"},{"F5","63"},
            {"F6","64"},{"F7","65"},{"F8","66"},{"F9","67"},{"F10","68"},
            {"F11","87"},{"F12","88"},{"META","125"},{"RIGHTALT","100"},
            {"MENU","127"},{"UP","103"},{"DOWN","108"},{"LEFT","105"},{"RIGHT","106"},
            {"HOME","102"},{"END","107"},{"DELETE","111"},{"SPACE","57"}
        };
        if (codes.contains(name))
            sendChord(codes[name], shiftActive());
        clearModifiers();
    }

    void pressShiftEnter() {
        sendChord("28", true);
        clearModifiers();
    }

    void pressClipboardShortcut(const QString &action) {
        static const QMap<QString, QString> codes {
            {"CUT", "45"}, {"COPY", "46"}, {"PASTE", "47"}
        };
        if (!codes.contains(action))
            return;
        runYdotool({"key", "29:1", codes[action] + ":1",
                    codes[action] + ":0", "29:0"});
        clearModifiers();
    }

    void sendChord(const QString &keyCode, bool includeShift) {
        QStringList arguments {"key"};
        QList<QString> modifiers;
        if (ctrlActive()) modifiers << "29";
        if (includeShift) modifiers << "42";
        if (altActive()) modifiers << "56";
        if (altGrActive()) modifiers << "100";
        if (metaActive()) modifiers << "125";
        for (const QString &code : modifiers)
            arguments << code + ":1";
        arguments << keyCode + ":1" << keyCode + ":0";
        for (auto it = modifiers.crbegin(); it != modifiers.crend(); ++it)
            arguments << *it + ":0";
        runYdotool(arguments);
    }

    void runYdotool(const QStringList &arguments) {
        auto *process = new QProcess(this);
        process->setStandardOutputFile(QProcess::nullDevice());
        process->setStandardErrorFile(QProcess::nullDevice());
        connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                process, &QObject::deleteLater);
        process->start("ydotool", arguments);
    }

    void saveSettings() {
        settings_.beginGroup("appearance");
        settings_.setValue("repeatDelay", repeatDelay_);
        settings_.setValue("repeatInterval", repeatInterval_);
        settings_.setValue("keySpacing", keySpacing_);
        settings_.setValue("borderWidth", borderWidth_);
        settings_.setValue("opacity", opacity_);
        settings_.setValue("keyColor", keyColor_);
        settings_.setValue("borderColor", borderColor_);
        settings_.endGroup();
        settings_.sync();
    }

    void refreshKeyboard(bool rebuild = false) {
        if (rebuild)
            buildKeys();
        applyStyle();
        opacityEffect_->setOpacity(opacity_ / 100.0);
        saveSettings();
    }

    void showOptions(QPushButton *menuButton) {
        auto *dialog = new QDialog;
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        dialog->setMinimumWidth(360);
        dialog->setStyleSheet(QString(R"(
            QDialog { background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #172333, stop:0.45 #0f1825, stop:1 #0a1018);
                color: #eef2f7; border: 1px solid #26384f; border-radius: 8px; }
            QLabel { color: #eef2f7; }
            #dragBar { background: %1; color: #607087; border: 1px solid %2;
                border-radius: 4px; font: 9px "Noto Sans"; }
            QSpinBox, QSlider, QPushButton {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 %1, stop:1 #182536);
                color: #ffffff; border: 1px solid %2; border-radius: 4px; padding: 4px;
            }
            QPushButton:hover { background: #2b3a50; border-color: #6b8db7; }
            QPushButton:pressed { background: #3a4c65; }
            QSlider::groove:horizontal { background: #101722; height: 6px; border-radius: 3px; }
            QSlider::handle:horizontal { background: #6b8db7; width: 14px; margin: -5px 0; border-radius: 7px; }
        )").arg(keyColor_, borderColor_));
        auto *outer = new QVBoxLayout(dialog);
        outer->setContentsMargins(4, 4, 4, 4);
        auto *titleBar = new QHBoxLayout;
        titleBar->addWidget(new DragBar(dialog, [dialog](QPoint, bool begin) {
            if (begin && dialog->windowHandle())
                dialog->windowHandle()->startSystemMove();
        }, true), 1);
        auto *close = new QPushButton("×", dialog);
        close->setFixedSize(22, 18);
        titleBar->addWidget(close);
        outer->addLayout(titleBar);
        QObject::connect(close, &QPushButton::clicked, dialog, &QDialog::close);
        auto *form = new QFormLayout;
        outer->addLayout(form);

        auto spin = [dialog](int value, int minimum, int maximum, const QString &suffix) {
            auto *control = new QSpinBox(dialog);
            control->setRange(minimum, maximum);
            control->setValue(value);
            control->setSuffix(suffix);
            return control;
        };
        auto *delay = spin(repeatDelay_, 100, 1500, " ms");
        auto *interval = spin(repeatInterval_, 15, 500, " ms");
        auto *spacing = spin(keySpacing_, 0, 12, " px");
        auto *border = spin(borderWidth_, 0, 6, " px");
        auto *opacity = new QSlider(Qt::Horizontal, dialog);
        opacity->setRange(20, 100);
        opacity->setValue(opacity_);
        auto *keyColor = new QPushButton(keyColor_, dialog);
        auto *borderColor = new QPushButton(borderColor_, dialog);

        form->addRow("Repeat delay", delay);
        form->addRow("Repeat interval", interval);
        form->addRow("Opacity", opacity);
        form->addRow("Key spacing", spacing);
        form->addRow("Border width", border);
        form->addRow("Key color", keyColor);
        form->addRow("Border color", borderColor);

        connect(delay, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
            repeatDelay_ = value; refreshKeyboard(true);
        });
        connect(interval, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
            repeatInterval_ = value; refreshKeyboard(true);
        });
        connect(spacing, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
            keySpacing_ = value; refreshKeyboard(true);
        });
        connect(border, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
            borderWidth_ = value; refreshKeyboard();
        });
        connect(opacity, &QSlider::valueChanged, this, [this](int value) {
            opacity_ = value; refreshKeyboard();
        });
        connect(keyColor, &QPushButton::clicked, dialog, [this, dialog, keyColor] {
            const QColor color = QColorDialog::getColor(QColor(keyColor_), dialog, "Key color");
            if (color.isValid()) { keyColor_ = color.name(); keyColor->setText(keyColor_); refreshKeyboard(); }
        });
        connect(borderColor, &QPushButton::clicked, dialog, [this, dialog, borderColor] {
            const QColor color = QColorDialog::getColor(QColor(borderColor_), dialog, "Border color");
            if (color.isValid()) { borderColor_ = color.name(); borderColor->setText(borderColor_); refreshKeyboard(); }
        });

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
        connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
        form->addRow(buttons);
        connect(dialog, &QDialog::destroyed, this, [menuButton] { menuButton->setEnabled(true); });
        dialog->show();
    }

    void applyStyle() {
        const QString style = QString(R"(
            #keyboard { background: transparent; }
            #keyboardPanel {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                            stop:0 #172333, stop:0.45 #0f1825, stop:1 #0a1018);
                border: 1px solid #26384f;
                border-radius: 8px;
            }
            #dragBar {
                background: %1;
                color: #607087;
                border: %2px solid %3;
                border-radius: 3px;
                font: 9px "Noto Sans";
            }
            #closeButton {
                background: #151f2d;
                color: #9eabbc;
                border: 1px solid #101722;
                border-radius: 3px;
                padding: 0;
                font: 12px "Noto Sans";
            }
            #closeButton:hover { background: #a53b48; color: white; }
            #closeButton:pressed { background: #cf4d5d; color: white; }
            QPushButton {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                            stop:0 %1, stop:1 #182536);
                color: #eef2f7;
                border: %2px solid %3;
                border-radius: 4px;
                padding: 0px 2px;
                font: 14px "Noto Sans";
            }
            QPushButton:hover {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                            stop:0 #3a506d, stop:1 #263a54);
                border-color: #6b8db7;
            }
            QPushButton:pressed {
                background: #3a4c65;
                padding-top: 2px;
            }
            QPushButton[special="true"] { color: #d8e0ea; font-size: 12px; }
            QPushButton[clipboardAction="CUT"],
            QPushButton[clipboardAction="COPY"],
            QPushButton[clipboardAction="PASTE"] { font-size: 18px; }
            QPushButton[modifierPressed="true"],
            QPushButton[activeModifier="true"] {
                background: #5576a3;
                color: white;
                border-color: #8db8ef;
            }
            QPushButton[lockedModifier="true"] {
                background: #9b3441;
                color: white;
                border-color: #f0808d;
            }
        )").arg(keyColor_).arg(borderWidth_).arg(borderColor_);
        setStyleSheet(style);
        opacityEffect_->setOpacity(opacity_ / 100.0);
    }
};

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setApplicationName("orbit-osk");
    app.setOrganizationName("Orbit");
    app.setQuitOnLastWindowClosed(true);
    Keyboard keyboard;
    keyboard.show();
    return app.exec();
}
