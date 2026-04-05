# virgil

proof-of-concept replacement preloader for DSPG DVF-series SoCs

## building

```sh
# clone with submodules
git clone https://github.com/gshax/virgil.git --recursive

cd libgsfw

# build mkboot
make tools

# build libgsfw (header parsing and min libc)
make EMBEDDED=1 libgsfw

cd ..

# build virgil
make
```
