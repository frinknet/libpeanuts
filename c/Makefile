# FINALLY!!!! An extensible Makefile that JUST WORKS!!!

REPO      ?= ghcr.io/frinknet/metabuild
IMAGE     ?= metabuild
ARCHES    ?= x64 x86 arm arm64 wasm wasi
COMPS     ?= clang gcc tcc xcc osx win

MKGOAL    := reshared respond
CFLAGS    ?= -Wno-unknown-warning-option -g -O0  -fno-omit-frame-pointer -fno-stack-protector
#CFLAGS   ?= -g -O0 -fno-stack-protector
CXXFLAGS  ?=
#LDFLAGS  ?= -s

OUTDIR    := out
SYSDIR    := syslib
EXTDIR    := lib libs vendor src/vendor
INCDIR    := include inc headers src/incs src/include src/headers
SRCDIR    := src source code srccode .
TPLDIR    := templates src/templates src/tpl tpl
SPLDIR    := examples demos demo samples src/examples src/demos experiments
DOCDIR    := docs doc manual documentation
WEBDIR    := web webroot wwwroot public static
CHKDIR    := tests test testing testcases check checks
NOTSRC    := CHKDIR EXTDIR SYSDIR TPLDIR SPLDIR DOCDIR WEBDIR

ifeq (,$(wildcard $(CURDIR)/.metabuild/metabuild.mk /metabuild/metabuild.mk))
 $(shell mkdir -p $(CURDIR)/.metabuild && curl -fsSL $(subst ghcr.io,github.com,$(REPO))/raw/main/metabuild.mk -o $(CURDIR)/.metabuild/metabuild.mk)
endif

# HOW?? Oh the secret is out... WE USE METABUILD!!! - (c) 2025 FRINKnet & Friends - 0BSD
include $(firstword $(wildcard $(CURDIR)/.metabuild/metabuild.mk /metabuild/metabuild.mk))
