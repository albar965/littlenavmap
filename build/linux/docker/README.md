# create image
```bash
BASE_IMAGE="ubuntu:24.04"
docker build --pull --tag="littlenavmap_build_${BASE_IMAGE}" --file=Dockerfile.deb.build --build-arg="BASE_IMAGE=${BASE_IMAGE}" .
```

example values of `BASE_IMAGE`:
* ubuntu:24.04
* ubuntu:22.04
* debian:bookworm

# build littlenavmap
```bash
PATH_TO_APROJECTS="/home/sweethome/git/forks/aprojects"
docker container run -it --rm --user `id -u`:`id -g` --volume "${PATH_TO_APROJECTS}:/build" "littlenavmap_build_${BASE_IMAGE}"
```
