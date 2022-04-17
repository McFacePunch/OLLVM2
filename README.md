## Configure LLVM to add obfuscation support

```bash
git clone -b main https://github.com/sr-tream/obfuscator
cd obfuscator
git submodule update --init llvm-project
cd llvm-project
git apply ../obfuscator.patch
```

Now you can build `llvm-project` to get llvm with obfuscation.



Also, to increase build speed you can rebuild only `opt` utility (because obfuscator add passes to optimization pipeline). And replace `opt` in you toolchain.