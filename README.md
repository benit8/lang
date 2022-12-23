# lang

Toy and experiment programming language.

```
return fn (args, env) {
  println("Hello world! from {}, at {}", args.at(0), env.get("PWD"))
  return 0
}
```
For now you must return your `main` function from your entrypoint.

## Build

```sh
make
```

## Run

```sh
./lang samples/hello-world
```

## Todo
- Lexer
  - strings escape sequences
  - number bases
  - glyphs
- Parser
  - missing grammar rules
  - types
- Compiler
  - missing AST nodes
- Interpreter
  - Runtime errors from native functions
  - Backtracing (debug locations)
- OOP
  - Better method handling
- std lib
  - strings

- User-defined classes
- TypeChecker
- Modules/namespaces
- Fibers
- EventLoop (libuv)
