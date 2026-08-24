# Reduced reproducer for llvm/llvm-project#217858

Upstream issue: *"Clang C++23 Modules ICE with `std::views::zip | std::views::take`
in Imported Inline Functions"* (labelled `needs-reduction`).

The original report needs Windows, the MSVC ABI, MSVC's STL and CMake's
experimental `import std` support.  The files here reproduce the same crash with
three tiny translation units, no standard library at all, and on any target.

## Files

| file        | role                                                       |
|-------------|------------------------------------------------------------|
| `lib.cppm`  | module `Lib` — stands in for `import std` (MSVC's `<ranges>`) |
| `mod.cppm`  | module `Mod` — stands in for the reporter's module `Inline` |
| `main.cpp`  | the importing translation unit that crashes clang           |
| `repro.sh`  | driver: `./repro.sh [clang++] [target-triple]`               |

```
$ ./repro.sh
+ clang++ --target=x86_64-pc-windows-msvc -std=c++23 -x c++-module lib.cppm --precompile -o Lib.pcm
+ clang++ ... -x c++-module mod.cppm --precompile -o Mod.pcm
+ clang++ ... -c main.cpp -o main.o
<crash>
```

## How this maps onto the original report

The crash signature in the report is

```
LLVM IR generation of declaration 'std::ranges::_Tuple_for_each_closure'
Mangling declaration 'std::ranges::_Tuple_for_each_closure'
Exception Code: 0xC0000005
```

`std::ranges::_Tuple_for_each_closure` is an MSVC STL internal helper in
`<ranges>` (used by `zip_view`):

```c++
template <class _CallbackType>
constexpr auto _Tuple_for_each_closure(_CallbackType& _Callback) noexcept {
    return [&_Callback]<class... _ViewTupleTypes>(_ViewTupleTypes&&... _View_tuples)
        noexcept(noexcept(((void)(_STD invoke(_Callback,
                    _STD forward<_ViewTupleTypes>(_View_tuples))), ...))) {
        ((void)(_STD invoke(_Callback, _STD forward<_ViewTupleTypes>(_View_tuples))), ...);
    };
}
```

i.e. **a function template with a deduced (`auto`) return type that returns a
lambda**, which is reached from the **`noexcept` specifier** of
`zip_view::_Iterator::operator++`:

```c++
constexpr _Iterator& operator++() noexcept(noexcept(_Tuple_for_each(
    [](auto& _Itr) static noexcept(noexcept(++_Itr)) { ++_Itr; }, _Current))) { ... }
```

`lib.cppm` keeps exactly that shape and drops everything else (tuple, `apply`,
`invoke`, `forward`, variadics, the `static` lambda, the body of `operator++`).
`std::views::take` turns out to be irrelevant; what matters is the deferred
exception specification that pulls in the lambda-returning template.

## What is required to reproduce

Verified by removing one ingredient at a time:

* **Two named modules.** `Mod` must `import` another *named module* that owns
  the lambda-returning template.  Turning `lib.cppm` into a header that
  `mod.cppm` `#include`s makes the crash go away.  (In the report those two
  modules are `std` and `Inline`.)
* **The lambda-returning function template must be reached through a deferred
  `noexcept` specifier.** Reaching the same lambda through a `decltype` in a
  member declaration does not crash.
* **The importing TU must actually call the function** whose body is in the
  BMI; a `main` that only imports `Mod` compiles fine.
* **The call must go through `operator++`** (the overloaded-operator call path);
  renaming it to an ordinary member function makes the crash go away.
* Not ABI specific — reproduces with `x86_64-pc-linux-gnu` as well as
  `x86_64-pc-windows-msvc`.  The report says Linux passes because libstdc++ /
  libc++ implement `zip_view` differently, not because of the mangler.
* Not C++23 specific — also crashes with `-std=c++20`.
* Also crashes without `inline` on `test()` in this reduction (the report's
  non-`inline` case passes only because nothing forces the body into the
  importer there).

## Diagnosis

Assertions build of clang 24.0.0git (`clang/lib/Serialization/ASTReaderDecl.cpp:2106`):

```
Assertion `!DD.IsLambda && !MergeDD.IsLambda && "faked up lambda definition?"' failed.
```

with (elided) stack:

```
Sema::MarkFunctionReferenced
  FunctionDecl::getBody
    ASTReader::GetExternalDeclStmt
      ASTReader::ReadStmtFromStream           <- body of Mod's test()
        ASTStmtReader::VisitDeclRefExpr
          ASTReader::GetType
            readFunctionProtoType
              readExceptionSpecInfo           <- deferred noexcept specifier
                ASTReader::ReadStmtFromStream
                  ASTStmtReader::VisitLambdaExpr
                    ASTReader::GetType
                      ASTReader::GetDecl      <- the lambda's closure type
                        ASTDeclReader::VisitCXXRecordDeclImpl
                          ASTDeclMerger::MergeDefinitionData  <- assert
```

Deserializing the exception-specification expression re-enters declaration
deserialization for the lambda's closure type, whose `DefinitionData` had
already been "faked up" by `ASTDeclReader::getOrFakePrimaryClassDefinition`
(`ASTReaderDecl.cpp`), which records it in `PendingFakeDefinitionData` on the
assumption that a later update record will replace it with the real definition.
For this lambda that never happens, so the closure type is left with empty,
non-lambda definition data.  Letting that assertion through (patched out
locally) then trips the follow-on

```
clang/lib/Serialization/ASTReader.cpp:10803:
Assertion `PendingFakeDefinitionData.empty() && "faked up a class definition but never saw the real one"' failed.
```

In a release build (no assertions) nothing stops the half-initialised closure
type from reaching IR generation, which is where the reporter's
`0xC0000005` / `SIGSEGV` inside "Mangling declaration ..." comes from.  Same
family as the already-fixed #120277 ("undeduced type in IR-generation") — the
deduced return type of the `*_closure` template is only filled in by
`ASTContext::adjustDeducedFunctionResultType` running out of
`ASTReader::FinishedDeserializing`, which can re-enter
`PassInterestingDeclsToConsumer` and hand the decl to CodeGen too early.
