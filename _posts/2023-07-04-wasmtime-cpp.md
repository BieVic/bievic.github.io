---
title:  "Embedding Wasmtime in C++"
categories: 
- posts
date: 2023-07-04
excerpt: "A little guide on how to embed Wasmtime in your C++ project."
tags:
---

This is a little guide on how to execute WebAssembly inside C++, and vice versa. 
First, we are going to embed [Wasmtime](https://wasmtime.dev/) in C++. This will allow us to call WebAssembly within any C++ project.
Wasmtime is a fantastic standalone runtime for WebAssembly, that comes equipped with it's own compiler, called Cranelift.
I might be a bit biased though as I have contributed to Cranelift/Wasmtime. Nevertheless, give it a try and if you have any questions, issue, etc., 
join us at [zulip](https://bytecodealliance.zulipchat.com/).

Anyway, let's get back to work. Calling C++ functions from WebAssembly is a little bit more tricky. 
This will require us to register Host-Functions in Wasmtime. Lastly, if we are interested in passing more complex data 
to WebAssembly (such as a C++ pointers or classes), we can utilize WebAssembly's *externref* type.

## Embedding WebAssembly

Before we can use Wasmtime in our C++ project, we need to include the [wasmtime.hh](https://github.com/bytecodealliance/wasmtime-cpp) header file. 
Now, let's suppose we have a simple WebAssembly module called *wasm.wat* as following:

```sh
(module
  (func (export "execute") ()
  )
)
```

This module contains one empty exported function that is correct, callable and yet does nothing. We now want to call this function
from within our C++ project.

```cpp
#include <wasmtime.hh>

using namespace wasmtime;

int main() {
    Engine engine;
    Linker linker(engine);
    Store store(engine);

    auto module = Module::compile(engine, readFile("wasm.wat")).unwrap();
    auto instance = Instance::create(store, module, {}).unwrap();
    auto execute = std::get<Func>(*instance.get(store, "execute"));
    execute.call(store, {}).unwrap();
}
```




## Call C++ from WebAssembly


## Externref for complex data
