// [MPSL-Test]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef _TEST_MP_UTILS_H
#define _TEST_MP_UTILS_H

#include "./mpsl.h"

struct TestLog : public mpsl::OutputLog {
  virtual void log(const Message& msg) MPSL_NOEXCEPT {
    // TYPE    - Message type - Error, warning, debug, or dump.
    // LINE    - Line number of the code related to the message (if known).
    // COLUMN  - Column of the code related to the message (if known).
    // HEADER  - Message or dump header, can be understood as a message type
    //           with a bit more information than just the type. If the message
    //           is a dump then the header is a dump type.
    // CONTENT - Message or dump content. If it's just a message it always
    //           contains just a single line. Dumps are typically multiline.
    printf("%s", msg.getHeader().getData());

    if (!msg.isDump()) {
      printf(" %s", msg.getContent().getData());
      if (msg.hasPosition())
        printf(" at [%u:%u]", msg.getLine(), msg.getColumn());
      printf("\n");
    }
    else {
      printf("\n%s\n", msg.getContent().getData());
    }
  }
};

#endif // _TEST_MP_UTILS_H
