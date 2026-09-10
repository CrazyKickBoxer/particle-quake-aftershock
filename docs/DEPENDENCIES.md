# Dependency inventory

vkQuake: commit 4519111e8ee33e6d7570c0b483f0f45c2fac6d52, https://github.com/Novum/vkQuake. GPL notices retained in engine/LICENSE.txt and source headers. Bundled SDL3, codecs, Vulkan headers and other upstream dependencies retain their notices.

PhysX 5.6.0 and Blast: commit 2264315594478a9aa0bda3464761a666fa107d76, tag 107.0-physx-5.6.0, https://github.com/NVIDIA-Omniverse/PhysX. BSD-3-Clause at this pinned revision. Licenses inspected before linking: dependencies/physx/LICENSE.md and blast/PACKAGE-LICENSES/blast-sdk-LICENSE.md. Initial build uses CPU PhysX and Blast low-level sources. No CUDA runtime or GPU binaries are redistributed.

Local PhysX build adjustment: copy freeglut only when snippets or explicit external-DLL copying are enabled. Core CPU physics does not use freeglut.

glslang 16.5.0 Windows release: https://github.com/KhronosGroup/glslang/releases/tag/16.5.0. Build-time tool only; its package retains its own licenses. glslang.exe is copied to the historical glslangValidator.exe name expected by upstream build rules.

Quake game assets are supplied externally from the user's Steam installation. No game assets are copied into source or distribution.

## Runtime and source notices

The Windows build retains upstream's bundled SDL 3.4.12 and audio codec binaries. The runtime package explicitly includes SDL3, FLAC, mpg123, Ogg, Opus, Opusfile, Vorbis, Vorbisfile and XMP. Unused libmad and libmikmod DLLs are excluded from the runtime archive. System Vulkan and the Microsoft runtime are supplied by the installed driver/toolchain; no CUDA binaries are packaged.

The pinned engine contains the SDL zlib notice and mimalloc MIT license, copied into package notices. FLAC's bundled header carries its BSD terms. Bundled Opus headers carry their BSD terms. Ogg/Vorbis and Opusfile notices are preserved in `docs/notices`, obtained from the primary [Ogg](https://github.com/xiph/ogg/blob/v1.3.2/COPYING), [Vorbis](https://github.com/xiph/vorbis/blob/v1.3.5/COPYING), and [Opusfile](https://github.com/xiph/opusfile/blob/v0.12/COPYING) repositories. These notice references do not establish the exact source revision of every prebuilt upstream DLL.

XMP identifies itself as 4.7.2 in the bundled header; its [release README](https://github.com/libxmp/libxmp/blob/libxmp-4.7.2/README) contains the MIT notice. The engine's codec configuration notes identify FLAC as 1.3.0 plus unspecified fixes and mpg123 as 1.22.4 with the documented decoder configuration. The mpg123 LGPL copying terms and the original [1.22.4 source archive](https://www.mpg123.de/download/mpg123-1.22.4.tar.bz2) are supplied with the source snapshot; SHA-256 `5069e02e50138600f10cc5f7674e44e9bf6f1930af81d0e1d2f869b3c0ee40d2`. Upstream's configuration notes remain in `engine/Windows/codecs/include`.

The local package includes the modified engine/integration source, source hashes, notices, and dependency pins. A fully reproducible rebuild/provenance audit of every upstream prebuilt codec is still outstanding. Do not interpret the local package as evidence that this release has completed that audit or a public distribution review.
