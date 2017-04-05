/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FilePrintStream_h
#define FilePrintStream_h

#include "wtf/Compiler.h"
#include "wtf/PrintStream.h"
#include <memory>
#include <stdio.h>

namespace WTF {

class WTF_EXPORT FilePrintStream final : public PrintStream {
 public:
  enum AdoptionMode { Adopt, Borrow };

  FilePrintStream(FILE*, AdoptionMode = Adopt);
  ~FilePrintStream() override;

  static std::unique_ptr<FilePrintStream> open(const char* filename,
                                               const char* mode);

  FILE* file() { return m_file; }

  PRINTF_FORMAT(2, 0) void vprintf(const char* format, va_list) override;
  void flush() override;

 private:
  FILE* m_file;
  AdoptionMode m_adoptionMode;
};

}  // namespace WTF

using WTF::FilePrintStream;

#endif  // FilePrintStream_h
