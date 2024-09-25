This project is an improved Obfuscating LLVM, OLLVM2. Now with better string encryption and soon with more functionality like variable obfuscation.

The new major version of llvm should be out in a few months as well.


## Configure LLVM to add obfuscation support

```bash
git clone -b main https://github.com/sr-tream/obfuscator
cd obfuscator
git submodule update --init llvm-project
cd llvm-project
git apply ../obfuscator.patch
```

Now we can build `llvm-project` to get llvm with obfuscation. I'll turn the chagnges into patches later.

```
cp inline_str_crypt.cpp llvm-project/llvm/lib/Transforms/Obfuscation/StringObfuscation.cpp

./build.sh
```
This will create a `./build` directory using ninja.

Now either build all of llvm, or specify the tool like `opt`. All changes are contained within opt currently but this may change in the future.

If build setup doesn't work due to lld missing, youre probably on a mac and used homebrew to install llvm. In this case add the following to your bashrz/zshrc `export PATH="$(brew --prefix llvm)/bin:$PATH"`

```
cd ./build

ninja -j 8 opt

```

Or you can build the standalone ctors string decryptor version which uses llvm as an sdk.

```
standalone-build.sh
```
