{
  description = "AnyPS5 relinker and reimplemented system libraries";

  inputs = {
    self.submodules = true;
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
      common = {
        version = self.shortRev or self.dirtyShortRev or "unstable";
        src = self;
        nativeBuildInputs = with pkgs; [ cmake ninja python3 ];
        cmakeFlags = [ "-DBUILD_TESTING=OFF" ];
        meta = {
          homepage = "https://github.com/boykopovar/AnyPS5";
          platforms = [ system ];
        };
      };
      ffmpeg = pkgs.fetchzip {
        url = "https://github.com/KytyPS5/ext-ffmpeg-core/releases/download/9ac4cfd195f1/ffmpeg-linux-x64.zip";
        hash = "sha256-MX8Rifr6CvFFkm0IAmJPNGzLb8FG5HJJUKO/l0zegko=";
        stripRoot = false;
      };
      relinker = pkgs.stdenv.mkDerivation (common // {
        pname = "anyps5-relinker";
        buildInputs = [ pkgs.glibc.static ];
        cmakeFlags = common.cmakeFlags ++ [ "-DANYPS5_RELINKER_ONLY=ON" ];
        ninjaFlags = [ "relinker" ];
        installPhase = ''
          runHook preInstall
          install -Dm755 core/relinker/relinker "$out/bin/relinker"
          runHook postInstall
        '';
        meta = common.meta // {
          description = "Relink PS5 executables into native host programs";
          mainProgram = "relinker";
        };
      });
      libs = pkgs.stdenv.mkDerivation (common // {
        pname = "anyps5-libs";
        nativeBuildInputs = common.nativeBuildInputs ++ (with pkgs; [
          autoPatchelfHook
          git
          pkg-config
          wayland-scanner
        ]);
        buildInputs = with pkgs; [
          alsa-lib
          dbus
          libdecor
          libffi
          libGL
          libx11
          libxcb
          libxcursor
          libxext
          libxi
          libxkbcommon
          libxrandr
          libxscrnsaver
          libxfixes
          libxrender
          pipewire
          pulseaudio
          vulkan-loader
          wayland
          wayland-protocols
        ];
        postPatch = ''
          substituteInPlace CMakeLists.txt \
            --replace-fail ' -static -static-libgcc -static-libstdc++' ""
        '';
        cmakeFlags = common.cmakeFlags ++ [ "-DFFMPEG_PREBUILT_DIR=${ffmpeg}" ];
        ninjaFlags = [ "libs" ];
        appendRunpaths = [ "$ORIGIN" "${pkgs.vulkan-loader}/lib" "${pkgs.dbus.lib}/lib" ];
        autoPatchelfIgnoreMissingDeps = [ "*.prx" ];
        dontStrip = true;
        installPhase = ''
          runHook preInstall
          mkdir -p "$out/lib/anyps5" "$out/share/licenses/anyps5/ffmpeg"
          cp core/libs/libs/*.prx "$out/lib/anyps5/"
          cp -r ${ffmpeg}/share/ffmpeg/. "$out/share/licenses/anyps5/ffmpeg/"
          runHook postInstall
        '';
        preFixup = ''
          for library in "$out"/lib/anyps5/*.prx; do
            while IFS= read -r dependency; do
              case "$dependency" in
                *.prx) test -f "$out/lib/anyps5/$dependency" || exit 1 ;;
              esac
            done < <(patchelf --print-needed "$library")
          done
        '';
        meta = common.meta // {
          description = "Patched AnyPS5 system libraries";
        };
      });
    in {
      packages.${system} = {
        inherit relinker libs;
        default = pkgs.symlinkJoin {
          name = "anyps5-${common.version}";
          paths = [ relinker libs ];
          meta = common.meta // { mainProgram = "relinker"; };
        };
      };
      formatter.${system} = pkgs.nixfmt;
    };
}
