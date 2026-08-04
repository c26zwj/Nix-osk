# Orbit OSK

A compact, touch-friendly on-screen keyboard for NixOS running KDE Plasma on
Wayland. KDE's layer-shell interface keeps keyboard focus in the target app
while still allowing pointer clicks, and text is sent through the kernel's
uinput interface using `ydotool`. KWin intentionally does not expose the native
virtual-keyboard protocol used by `wtype` to ordinary applications.

## Try it

```sh
nix run .
```

Pin it to Plasma's task manager or launch it from the application menu as
**Orbit On-Screen Keyboard**. It opens centered against the bottom screen edge.

## Install in NixOS

Add the flake to your system inputs, import its NixOS module, and add your login
user to the `ydotool` group:

```nix
{
  inputs.orbit-osk.url = "path:/home/you/path/to/osk";

  outputs = inputs@{ nixpkgs, orbit-osk, ... }: {
    nixosConfigurations.your-host = nixpkgs.lib.nixosSystem {
      system = "x86_64-linux";
      specialArgs = { inherit inputs; };
      modules = [
        orbit-osk.nixosModules.default
        {
          users.users.your-user.extraGroups = [ "ydotool" ];
        }
      ];
    };
  };
}
```

Rebuild with `sudo nixos-rebuild switch --flake .#your-host`, then **log out and
back in** so the new group membership takes effect. The module starts the
restricted `ydotoold` service, sets `YDOTOOL_SOCKET`, and installs Orbit.

For a quick manual configuration without importing the module:

```nix
programs.ydotool.enable = true;
users.users.your-user.extraGroups = [ "ydotool" ];
```

`nix run .` by itself can launch the interface, but typing requires that system
service and group membership because `/dev/uinput` is privileged by design.

## Controls

- Shift is one-shot; the next printable key uses its shifted value.
- Drag the small dotted bar above the keys to reposition the keyboard.
- Hold printable keys, Space, or Backspace for key repeat.
- Open **Menu** to configure repeat timing, colors, opacity, spacing, and borders.
- Function keys, Backspace, Tab, Caps Lock, Enter, modifiers, Space, and the
  two arrow keys send native key events.
- Close the keyboard from the task manager or application launcher.
