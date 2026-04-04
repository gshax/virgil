# virgil

proof-of-concept replacement preloader for DSPG DVF-series SoCs

## building

```sh
# clone with submodules
git clone https://github.com/gshax/virgil.git --recursive

# build mkboot
cd libgsfw
make build/native/mkboot
cd ..

# build virgil
make
```
