---
title:  "Embedding Wasmtime in C++"
category: posts
date: 2023-07-04
excerpt: "A little guide on how to embed Wasmtime in your C++ project."
---

I think by now we all can agree, that WebAssembly is an exciting technology

# WebAssembly and C++

If you're interested in executing WebAssembly in your C++ project,
you have plenty of sound options to do do. Most notably, [Wasmer](https://wasmer.io/), 
[WAMR](https://github.com/bytecodealliance/wasm-micro-runtime), [Wasmtime](https://wasmtime.dev/), just to name a few.
Now, let's have a look at Wasmtime, my go-to standalone WebAssembly engine. You might ask: Why Wasmtime?

Well, I had a great experience using it for my Master's thesis, it's easy to use and achieves excellent performance.
The reason why I'm writing this post is the unfortunate lack of documentation on how to embed Wasmtime in C++.
So, without further ado, let us have a look how you can do it!

#
