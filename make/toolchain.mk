ifeq ($(OS),Windows_NT)
SHELL := cmd.exe
.SHELLFLAGS := /c

VSWHERE := C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe
VSDIR   := $(shell "$(VSWHERE)" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath)
VSDEV   := $(VSDIR)\Common7\Tools\VsDevCmd.bat
DEVCMD  := call "$(VSDEV)" -arch=x64 -host_arch=x64 -no_logo &&

LLVMPATH := set "PATH=$(VSDIR)\VC\Tools\Llvm\x64\bin;%PATH%" &&
LLVM     :=

BOOTSTRAP := powershell -NoProfile -ExecutionPolicy Bypass -File scripts\bootstrap.ps1
GIT_BASH  ?= C:\Program Files\Git\bin\bash.exe
RUNSH     := $(DEVCMD) set "PWD=" && "$(GIT_BASH)"
QUICHE_FOR := $(RUNSH) scripts/build-quiche.sh
QUICHE    := $(QUICHE_FOR) windows
OPUS_FOR  := $(RUNSH) scripts/build-opus.sh
OPUS      := $(OPUS_FOR) windows
PYTHON    := python
NULDEV    := nul
RMRF      := $(DEVCMD) cmake -E rm -rf
HELPCAT   := type make\help.txt
else
UNAME  := $(shell uname -s)
DEVCMD :=

LLVMPATH :=
LLVM     := $(if $(filter Darwin,$(UNAME)),xcrun)

BOOTSTRAP := scripts/bootstrap.sh
RUNSH     :=
QUICHE_FOR := scripts/build-quiche.sh
QUICHE    := $(QUICHE_FOR) host
OPUS_FOR  := scripts/build-opus.sh
OPUS      := $(OPUS_FOR) host
PYTHON    := python3
NULDEV    := /dev/null
RMRF      := rm -rf
HELPCAT   := cat make/help.txt
endif
