// SPDX-FileCopyrightText: 2017-2022 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

// if linking with the static libCZI-library, the variable "_LIBCZISTATICLIB" should be defined.
#if !defined(_LIBCZISTREAMSTATICLIB)

// Exporting symbols to the shared libraries
#ifdef LIBCZI_EXPORTS
    #ifdef __GNUC__
        #define LIBCZISTREAMS_API __attribute__ ((visibility ("default")))
    #else
        #define LIBCZISTREAMS_API __declspec(dllexport)
    #endif
#else
// Importing symbols for the shared libraries
    #ifdef __GNUC__
        #define LIBCZISTREAMS_API
    #else
        #define LIBCZISTREAMS_API __declspec(dllimport)
    #endif
#endif

#else
// static library doesn't need import/export.
#define LIBCZISTREAMS_API 
#endif
