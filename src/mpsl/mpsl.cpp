// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define MPSL_EXPORTS

// [Dependencies - MPSL]
#include "./mpast_p.h"
#include "./mpastoptimizer_p.h"
#include "./mpcodegen_p.h"
#include "./mpatomic_p.h"
#include "./mpformatutils_p.h"
#include "./mpir_p.h"
#include "./mpirpass_p.h"
#include "./mpirtox86_p.h"
#include "./mplang_p.h"
#include "./mpparser_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

// ============================================================================
// [mpsl::mpAssertionFailed]
// ============================================================================

void mpAssertionFailed(const char* exp, const char* file, int line) noexcept {
  ::fprintf(stderr, "Assertion failed: %s\n, file %s, line %d\n", exp, file, line);
  ::abort();
}

// ============================================================================
// [mpsl::mpTraceError]
// ============================================================================

Error mpTraceError(Error error) noexcept {
  return error;
}

// ============================================================================
// [mpsl::Helpers]
// ============================================================================

template<typename T>
MPSL_INLINE T* mpObjectAddRef(T* self) noexcept {
  if (self->_refCount == 0)
    return self;

  mpAtomicInc(&self->_refCount);
  return self;
}

template<typename T>
MPSL_INLINE void mpObjectRelease(T* self) noexcept {
  if (self->_refCount != 0 && !mpAtomicDec(&self->_refCount))
    self->destroy();
}

// Declared in public "mpsl.h" header.
MPSL_INLINE void Context::Impl::destroy() noexcept {
  RuntimeData* rt = static_cast<RuntimeData*>(_runtimeData);

  mpObjectRelease(rt);
  ::free(this);
}

MPSL_INLINE void Program::Impl::destroy() noexcept {
  RuntimeData* rt = static_cast<RuntimeData*>(_runtimeData);
  rt->_runtime.release(_main);

  mpObjectRelease(rt);
  ::free(this);
}

// ============================================================================
// [mpsl::Layout - Internal]
// ============================================================================

static MPSL_INLINE bool mpLayoutIsEmbedded(Layout* self, const uint8_t* data) noexcept {
  return self->_embeddedData == data;
}

static MPSL_INLINE uint32_t mpLayoutGetRemainingSize(const Layout* self) noexcept {
  return self->_dataIndex - static_cast<uint32_t>(self->_membersCount * sizeof(Layout::Member));
}

static MPSL_INLINE Layout::Member* mpLayoutFind(
  const Layout* self, const char* name, size_t size) noexcept {

  Layout::Member* m = self->_members;
  uint32_t count = self->_membersCount;

  for (uint32_t i = 0; i < count; i++)
    if (m[i].nameSize == size && (size == 0 || ::memcmp(m[i].name, name, size) == 0))
      return &m[i];

  return nullptr;
}

static MPSL_NOINLINE Error mpLayoutResize(Layout* self, uint32_t dataSize) noexcept {
  uint8_t* oldData = self->_data;
  uint8_t* newData = static_cast<uint8_t*>(::malloc(dataSize));

  if (newData == nullptr)
    return MPSL_TRACE_ERROR(kErrorNoMemory);

  uint32_t dataIndex = dataSize;
  uint32_t count = self->_membersCount;

  Layout::Member* oldMember = reinterpret_cast<Layout::Member*>(oldData);
  Layout::Member* newMember = reinterpret_cast<Layout::Member*>(newData);

  uint32_t size = self->_nameSize;
  if (size != 0) {
    dataIndex -= ++size;

    ::memcpy(newData + dataIndex, self->_name, size);
    self->_name = reinterpret_cast<char*>(newData + dataIndex);
  }

  for (uint32_t i = 0; i < count; i++, oldMember++, newMember++) {
    size = oldMember->nameSize;
    newMember->nameSize = size;
    MPSL_ASSERT(size < dataIndex);

    dataIndex -= ++size;
    MPSL_ASSERT(dataIndex >= (i + 1) * sizeof(Layout::Member));

    ::memcpy(newData + dataIndex, oldMember->name, size);
    newMember->name = reinterpret_cast<char*>(newData + dataIndex);
    newMember->typeInfo = oldMember->typeInfo;
    newMember->offset = oldMember->offset;
  }

  self->_data = newData;
  self->_dataSize = dataSize;
  self->_dataIndex = dataIndex;

  if (oldData && !mpLayoutIsEmbedded(self, oldData))
    ::free(oldData);

  return kErrorOk;
}

static MPSL_INLINE Error mpLayoutPrepareAdd(Layout* self, size_t n) noexcept {
  size_t remainingSize = mpLayoutGetRemainingSize(self);
  if (remainingSize < n) {
    uint32_t newSize = self->_dataSize;
    // Make sure we reserve much more than the current size if it wasn't enough.
    if (newSize <= 128)
      newSize = 512;
    else
      newSize *= 2;
    MPSL_PROPAGATE(mpLayoutResize(self, newSize));
  }
  return kErrorOk;
}

// ============================================================================
// [mpsl::Layout - Construction / Destruction]
// ============================================================================

Layout::Layout() noexcept
  : _data(nullptr),
    _name(nullptr),
    _nameSize(0),
    _membersCount(0),
    _dataSize(0),
    _dataIndex(0) {}

Layout::Layout(uint8_t* data, uint32_t dataSize) noexcept
  : _data(data),
    _name(nullptr),
    _nameSize(0),
    _membersCount(0),
    _dataSize(dataSize),
    _dataIndex(dataSize) {}

Layout::~Layout() noexcept {
  uint8_t* data = _data;

  if (data && !mpLayoutIsEmbedded(this, data))
    ::free(data);
}

// ============================================================================
// [mpsl::Layout - Interface]
// ============================================================================

Error Layout::_configure(const char* name, size_t size) noexcept {
  if (size == Globals::kInvalidIndex)
    size = name ? ::strlen(name) : (size_t)0;

  if (size > Globals::kMaxIdentifierLength)
    return MPSL_TRACE_ERROR(kErrorInvalidArgument);

  if (_name)
    return MPSL_TRACE_ERROR(kErrorAlreadyConfigured);

  size++; // Include NULL terminator.
  MPSL_PROPAGATE(mpLayoutPrepareAdd(this, size));

  uint32_t dataIndex = _dataIndex - static_cast<uint32_t>(size);
  ::memcpy(_data + dataIndex, name, size);

  _name = reinterpret_cast<char*>(_data + dataIndex);
  // Casting to `uint32_t` is safe, see `kMaxIdentifierLength`, store size
  // without a NULL terminator (hence `-1`).
  _nameSize = static_cast<uint32_t>(size - 1);

  _dataIndex = dataIndex;
  return kErrorOk;
}

const Layout::Member* Layout::_get(const char* name, size_t size) const noexcept {
  if (name == nullptr)
    return nullptr;

  if (size == Globals::kInvalidIndex)
    size = ::strlen(name);

  return mpLayoutFind(this, name, size);
}

Error Layout::_add(const char* name, size_t size, uint32_t typeInfo, int32_t offset) noexcept {
  if (name == nullptr)
    return MPSL_TRACE_ERROR(kErrorInvalidArgument);

  if (size == Globals::kInvalidIndex)
    size = ::strlen(name);

  if (size > Globals::kMaxIdentifierLength)
    return MPSL_TRACE_ERROR(kErrorInvalidArgument);

  uint32_t count = _membersCount;
  if (count >= Globals::kMaxMembersCount)
    return MPSL_TRACE_ERROR(kErrorTooManyMembers);

  Member* member = mpLayoutFind(this, name, size);
  if (member)
    return MPSL_TRACE_ERROR(kErrorAlreadyExists);

  MPSL_PROPAGATE(mpLayoutPrepareAdd(this, size + 1 + static_cast<uint32_t>(sizeof(Member))));
  member = _members + count;
  uint32_t dataIndex = _dataIndex - (static_cast<uint32_t>(size) + 1);

  ::memcpy(_data + dataIndex, name, size + 1);
  member->name = reinterpret_cast<char*>(_data + dataIndex);
  member->nameSize = static_cast<uint32_t>(size);
  member->typeInfo = typeInfo;
  member->offset = offset;

  _membersCount++;
  _dataIndex = dataIndex;
  return kErrorOk;
}

// ============================================================================
// [mpsl::Context - Construction / Destruction]
// ============================================================================

static const Context::Impl mpContextNull = { 0, nullptr };

Context::Context() noexcept
  : _d(const_cast<Impl*>(&mpContextNull)) {}

Context::Context(const Context& other) noexcept
  : _d(mpObjectAddRef(other._d)) {}

Context::~Context() noexcept {
  mpObjectRelease(_d);
}

Context Context::create() noexcept {
  Impl* d = static_cast<Impl*>(::malloc(sizeof(Impl)));

  if (d == nullptr) {
    // Allocation failure.
    d = const_cast<Impl*>(&mpContextNull);
  }
  else {
    RuntimeData* rt = static_cast<RuntimeData*>(::malloc(sizeof(RuntimeData)));
    if (rt == nullptr) {
      // Allocation failure.
      ::free(d);
      d = const_cast<Impl*>(&mpContextNull);
    }
    else {
      d->_refCount = 1;
      d->_runtimeData = new(rt) RuntimeData();
    }
  }

  return Context(d);
}

// ============================================================================
// [mpsl::Context - Reset]
// ============================================================================

Error Context::reset() noexcept {
  mpObjectRelease(mpAtomicSetXchgT<Impl*>(
    &_d, const_cast<Impl*>(&mpContextNull)));

  return kErrorOk;
}

// ============================================================================
// [mpsl::Context - Clone / Freeze]
// ============================================================================

Error Context::clone() noexcept {

  // TODO:
  return kErrorOk;
}

Error Context::freeze() noexcept {

  // TODO:
  return kErrorOk;
}

// ============================================================================
// [mpsl::Context - Compile]
// ============================================================================

#define MPSL_PROPAGATE_AND_HANDLE_COLLISION(...)                              \
  do {                                                                        \
    AstSymbol* collidedSymbol = nullptr;                                      \
    Error _errorValue = __VA_ARGS__;                                          \
                                                                              \
    if (MPSL_UNLIKELY(_errorValue)) {                                         \
      if (_errorValue == kErrorSymbolCollision && log) {                      \
        sbTmp.assignFormat("Built-in symbol collision: '%s' already defined", \
          collidedSymbol->name());                                            \
        log->log(OutputLog::Message(                                          \
          OutputLog::kMessageError, 0, 0,                                     \
          StringRef("ERROR", 5),                                              \
          StringRef(sbTmp.data(), sbTmp.size())));                            \
      }                                                                       \
      return _errorValue;                                                     \
    }                                                                         \
  } while (0)

Error Context::_compile(Program& program, const CompileArgs& ca, OutputLog* log) noexcept {
  const char* body = ca.body.data();
  size_t size = ca.body.size();

  uint32_t options = ca.options;
  uint32_t numArgs = ca.numArgs;

  if (numArgs == 0 || numArgs > Globals::kMaxArgumentsCount)
    return MPSL_TRACE_ERROR(kErrorInvalidArgument);

  // --------------------------------------------------------------------------
  // [Debug Strings]
  // --------------------------------------------------------------------------

  static const char kDebugHeadingAST[] = "AST";
  static const char kDebugHeadingIR[]  = "IR";
  static const char kDebugHeadingASM[] = "ASM";

  // --------------------------------------------------------------------------
  // [Init]
  // --------------------------------------------------------------------------

  // Init options first.
  options &= _kOptionsMask;

  if (log)
    options |= kInternalOptionLog;
  else
    options &= ~(kOptionVerbose | kOptionDebugAst | kOptionDebugIR | kOptionDebugASM);

  if (size == Globals::kInvalidIndex)
    size = ::strlen(body);

  Zone zone(32768 - Zone::kBlockOverhead);
  ZoneAllocator allocator(&zone);
  StringTmp<512> sbTmp;

  AstBuilder ast(&allocator);
  IRBuilder ir(&allocator, numArgs);

  MPSL_PROPAGATE(ast.addProgramScope());
  MPSL_PROPAGATE(ast.addBuiltInTypes(mpTypeInfo, kTypeCount));
  MPSL_PROPAGATE(ast.addBuiltInConstants(mpConstInfo, MPSL_ARRAY_SIZE(mpConstInfo)));
  MPSL_PROPAGATE(ast.addBuiltInIntrinsics());

  for (uint32_t slot = 0; slot < numArgs; slot++) {
    MPSL_PROPAGATE_AND_HANDLE_COLLISION(ast.addBuiltInObject(slot, ca.layout[slot], &collidedSymbol));
  }

  // Setup basic data structures used during parsing and compilation.
  ErrorReporter errorReporter(body, size, options, log);

  // --------------------------------------------------------------------------
  // [AST]
  // --------------------------------------------------------------------------

  // Parse the source code into AST.
  { MPSL_PROPAGATE(Parser(&ast, &errorReporter, body, size).parseProgram(ast.programNode())); }

  // Perform a semantic analysis of the parsed AST.
  //
  // It can add some nodes required by implicit casts and fail if the code is
  // semantically incorrect - for example invalid implicit cast, explicit-cast,
  // or function call. This pass doesn't do constant folding or optimizations.
  { MPSL_PROPAGATE(AstAnalysis(&ast, &errorReporter).onProgram(ast.programNode())); }

  if (options & kOptionDebugAst) {
    ast.dump(sbTmp);
    log->log(
      OutputLog::Message(
        OutputLog::kMessageDump, 0, 0,
        StringRef(kDebugHeadingAST, MPSL_ARRAY_SIZE(kDebugHeadingAST) - 1),
        StringRef(sbTmp.data(), sbTmp.size())));
    sbTmp.clear();
  }

  // Perform basic optimizations at AST level (dead code removal and constant
  // folding). This pass shouldn't do any unsafe optimizations and it's a bit
  // limited, but it's faster to do them now than doing these optimizations at
  // IR level.
  { MPSL_PROPAGATE(AstOptimizer(&ast, &errorReporter).onProgram(ast.programNode())); }

  if (options & kOptionDebugAst) {
    ast.dump(sbTmp);
    log->log(
      OutputLog::Message(
        OutputLog::kMessageDump, 0, 0,
        StringRef(kDebugHeadingAST, MPSL_ARRAY_SIZE(kDebugHeadingAST) - 1),
        StringRef(sbTmp.data(), sbTmp.size())));
    sbTmp.clear();
  }

  // --------------------------------------------------------------------------
  // [IR]
  // --------------------------------------------------------------------------

  // Translate AST to IR.
  {
    CodeGen::Result unused(false);
    MPSL_PROPAGATE(CodeGen(&ast, &ir).onProgram(ast.programNode(), unused));
  }

  if (options & kOptionDebugIR) {
    ir.dump(sbTmp);
    log->log(
      OutputLog::Message(
        OutputLog::kMessageDump, 0, 0,
        StringRef(kDebugHeadingIR, MPSL_ARRAY_SIZE(kDebugHeadingIR) - 1),
        StringRef(sbTmp.data(), sbTmp.size())));
    sbTmp.clear();
  }

  MPSL_PROPAGATE(mpIRPass(&ir));

  if (options & kOptionDebugIR) {
    ir.dump(sbTmp);
    log->log(
      OutputLog::Message(
        OutputLog::kMessageDump, 0, 0,
        StringRef(kDebugHeadingIR, MPSL_ARRAY_SIZE(kDebugHeadingIR) - 1),
        StringRef(sbTmp.data(), sbTmp.size())));
    sbTmp.clear();
  }

  // --------------------------------------------------------------------------
  // [ASM]
  // --------------------------------------------------------------------------

  // Compile and store the reference to the `main()` function.
  RuntimeData* rt = static_cast<RuntimeData*>(_d->_runtimeData);
  Program::Impl* programD = program._d;

  void* func = nullptr;
  {
    asmjit::StringLogger asmlog;
    asmjit::CodeHolder code;

    code.init(rt->_runtime.codeInfo());
    asmjit::x86::Compiler c(&code);

    if (options & kOptionDebugASM)
      code.setLogger(&asmlog);

    IRToX86 compiler(&allocator, &c);
    if (options & kOptionDisableSSE4_1)
      compiler._enableSSE4_1 = false;
    MPSL_PROPAGATE(compiler.compileIRAsFunc(&ir));

    asmjit::Error err = c.finalize();
    if (err) return MPSL_TRACE_ERROR(kErrorJITFailed);

    err = rt->_runtime.add(&func, &code);
    if (err) return MPSL_TRACE_ERROR(kErrorJITFailed);

    if (options & kOptionDebugASM)
      log->log(
        OutputLog::Message(
          OutputLog::kMessageDump, 0, 0,
          StringRef(kDebugHeadingASM, MPSL_ARRAY_SIZE(kDebugHeadingASM) - 1),
          StringRef(asmlog.data(), asmlog.size())));
  }

  if (programD->_refCount == 1 && static_cast<RuntimeData*>(programD->_runtimeData) == rt) {
    rt->_runtime.release(programD->_main);
    programD->_main = func;
  }
  else {
    programD = static_cast<Program::Impl*>(::malloc(sizeof(Program::Impl)));
    if (programD == nullptr) {
      rt->_runtime.release(func);
      return MPSL_TRACE_ERROR(kErrorNoMemory);
    }

    programD->_refCount = 1;
    programD->_runtimeData = mpObjectAddRef(rt);
    programD->_main = func;

    mpObjectRelease(
      mpAtomicSetXchgT<Program::Impl*>(
        &program._d, programD));
  }

  return kErrorOk;
}

// ============================================================================
// [mpsl::Context - Operator Overload]
// ============================================================================

Context& Context::operator=(const Context& other) noexcept {
  mpObjectRelease(
    mpAtomicSetXchgT<Impl*>(
      &_d, mpObjectAddRef(other._d)));

  return *this;
}

// ============================================================================
// [mpsl::Program - Construction / Destruction]
// ============================================================================

static const Program::Impl mpProgramNull = { 0, nullptr, nullptr };

Program::Program() noexcept
  : _d(const_cast<Program::Impl*>(&mpProgramNull)) {}

Program::Program(const Program& other) noexcept
  : _d(mpObjectAddRef(other._d)) {}

Program::~Program() noexcept {
  mpObjectRelease(_d);
}

// ============================================================================
// [mpsl::Program - Reset]
// ============================================================================

Error Program::reset() noexcept {
  mpObjectRelease(mpAtomicSetXchgT<Impl*>(
    &_d, const_cast<Impl*>(&mpProgramNull)));
  return kErrorOk;
}

// ============================================================================
// [mpsl::Program - Operator Overload]
// ============================================================================

Program& Program::operator=(const Program& other) noexcept {
  mpObjectRelease(
    mpAtomicSetXchgT<Impl*>(
      &_d, mpObjectAddRef(other._d)));

  return *this;
}

// ============================================================================
// [mpsl::OutputLog - Construction / Destruction]
// ============================================================================

OutputLog::OutputLog() noexcept {}
OutputLog::~OutputLog() noexcept {}

// ============================================================================
// [mpsl::ErrorReporter - Interface]
// ============================================================================

void ErrorReporter::getLineAndColumn(uint32_t position, uint32_t& line, uint32_t& column) noexcept {
  // Should't happen, but be defensive.
  if (static_cast<size_t>(position) >= _size) {
    line = 0;
    column = 0;
    return;
  }

  const char* pStart = _body;
  const char* p = pStart + position;

  uint32_t x = 0;
  uint32_t y = 1;

  // Find column.
  while (p[0] != '\n') {
    x++;
    if (p == pStart)
      break;
    p--;
  }

  // Find line.
  while (p != pStart) {
    y += p[0] == '\n';
    p--;
  }

  line = y;
  column = x;
}

void ErrorReporter::onWarning(uint32_t position, const char* fmt, ...) noexcept {
  if (reportsWarnings()) {
    StringTmp<256> sb;

    va_list ap;
    va_start(ap, fmt);
    FormatUtils::vformat(sb, fmt, ap);
    va_end(ap);

    onWarning(position, sb);
  }
}

void ErrorReporter::onWarning(uint32_t position, const String& msg) noexcept {
  if (reportsWarnings()) {
    uint32_t line, column;
    getLineAndColumn(position, line, column);
    _log->log(
      OutputLog::Message(
        OutputLog::kMessageWarning, line, column,
        StringRef("WARNING", 7),
        StringRef(msg.data(), msg.size())));
  }
}

Error ErrorReporter::onError(Error error, uint32_t position, const char* fmt, ...) noexcept {
  if (reportsErrors()) {
    StringTmp<256> sb;

    va_list ap;
    va_start(ap, fmt);
    FormatUtils::vformat(sb, fmt, ap);
    va_end(ap);

    return onError(error, position, sb);
  }
  else {
    return MPSL_TRACE_ERROR(error);
  }
}

Error ErrorReporter::onError(Error error, uint32_t position, const String& msg) noexcept {
  if (reportsErrors()) {
    OutputLog::Message logMsg(
      OutputLog::kMessageError, 0, 0,
      StringRef("ERROR", 5),
      StringRef(msg.data(), msg.size()));

    getLineAndColumn(position, logMsg._line, logMsg._column);
    _log->log(logMsg);
  }

  return MPSL_TRACE_ERROR(error);
}

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"
