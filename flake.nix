{
  description = "sox_ng with large sinc filter support (up to 1 billion taps)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
        };

        # Import our Nix expression from .flox/pkgs/
        sox_ng = pkgs.callPackage ./.flox/pkgs/sox_ng.nix { };
      in
      {
        packages = {
          default = sox_ng;
          sox_ng = sox_ng;
          # Alias for backwards compatibility
          sox-sinc = sox_ng;
        };

        apps.default = {
          type = "app";
          program = "${sox_ng}/bin/sox";
        };

        devShells.default = pkgs.mkShell {
          buildInputs = [ sox_ng ];
          inputsFrom = [ sox_ng ];
        };
      });
}
