#ifndef CORE_H
#define CORE_H

#ifdef __linux__
#define CORE_USE_LINUX
#elif _WIN32 // NOTE: This gets defined for both 32-bit and 64-bit Windows target.
#define CORE_USE_WINDOWS
#endif

#ifdef CORE_USE_WINDOWS
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOGDICAPMASKS
#define NOSYSMETRICS
#define NOMENUS
#define NOICONS
#define NOSYSCOMMANDS
#define OEMRESOURCE
#define NOATOM
#define NOCLIPBOARD
#define NOCOLOR
#define NOCTLMGR
#define NODRAWTEXT
#define NOKERNEL
#define NONLS
#define NOMEMMGR
#define NOMETAFILE
#define NOMINMAX
#define NOOPENFILE
#define NOSCROLL
#define NOSERVICE
#define NOSOUND
#define NOTEXTMETRIC
#define NOWH
#define NOCOMM
#define NOKANJI
#define NOHELP
#define NOPROFILER
#define NODEFERWINDOWPOS
#define NOMCX
#define NORPC
#define NOPROXYSTUB
#define NOIMAGE
#define NOTAPE
#define NOMINMAX
#define STRICT
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>

#endif // CORE_USE_WINDOWS

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#define core_minimum(x, y) ((x) < (y) ? (x) : (y))
#define core_maximum(x, y) ((x) > (y) ? (x) : (y))
#define core_swap(x, y, type) { type temp = (x); (x) = (y); (y) = temp; }
#define core_array_count(x) (sizeof(x)/sizeof((x)[0]))

#endif // CORE_H