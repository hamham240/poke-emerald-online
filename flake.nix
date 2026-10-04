{
  description = "Pokémon Emerald online multiplayer (pokeemerald-expansion fork)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";

    mgba-online = {
      url = "github:hamham240/mgba-online";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.flake-utils.follows = "flake-utils";
    };
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
      mgba-online,
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        inherit (pkgs) lib;

        rom = pkgs.stdenv.mkDerivation {
          pname = "pokeemerald-online";
          version = self.shortRev or self.dirtyShortRev or "dirty";

          src = lib.cleanSource self;

          nativeBuildInputs = [
            pkgs.gcc-arm-embedded
            pkgs.pkg-config
            pkgs.perl
          ];
          buildInputs = [
            pkgs.libpng
            pkgs.zlib
          ];

          enableParallelBuilding = true;
          makeFlags = [ "modern" ];

          installPhase = ''
            runHook preInstall
            install -Dm644 -t $out pokeemerald_modern.{gba,elf,map}
            runHook postInstall
          '';

          dontFixup = true;

          meta = {
            description = "Pokémon Emerald ROM with online multiplayer";
            platforms = lib.platforms.unix;
          };
        };
      in
      {
        packages = {
          inherit rom;
          default = rom;
        };

        devShells.default = pkgs.mkShell {
          inputsFrom = [ rom ];

          packages = lib.optional (mgba-online.packages ? ${system}) mgba-online.packages.${system}.default;

          LC_COLLATE = "C";
        };

        formatter = pkgs.nixfmt;
      }
    );
}
