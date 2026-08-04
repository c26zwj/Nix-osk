{
  description = "Orbit on-screen keyboard for NixOS Plasma Wayland";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      forAllSystems = nixpkgs.lib.genAttrs [ "x86_64-linux" "aarch64-linux" ];
    in {
      packages = forAllSystems (system:
        let pkgs = nixpkgs.legacyPackages.${system}; in {
          default = pkgs.stdenv.mkDerivation {
            pname = "orbit-osk";
            version = "0.1.0";
            src = self;
            nativeBuildInputs = [ pkgs.cmake pkgs.qt6.wrapQtAppsHook ];
            buildInputs = [ pkgs.qt6.qtbase pkgs.kdePackages.layer-shell-qt pkgs.ydotool ];
            postFixup = ''
              wrapProgram $out/bin/orbit-osk \
                --prefix PATH : ${pkgs.lib.makeBinPath [ pkgs.ydotool ]}
            '';
          };
        });

      apps = forAllSystems (system: {
        default = {
          type = "app";
          program = "${self.packages.${system}.default}/bin/orbit-osk";
        };
      });

      nixosModules.default = { pkgs, ... }: {
        programs.ydotool.enable = true;
        environment.systemPackages = [ self.packages.${pkgs.system}.default ];
      };
    };
}
