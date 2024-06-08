# create image
```bash
docker build --file=Dockerfile.ubuntu.build --build-arg="UBUNTU_VERSION=24" .
```

# build littlenavmap
```bash
PATH_TO_APROJECTS="~/git/forks/aprojects"
docker container run -it --rm --volume "${PATH_TO_APROJECTS}/littlenavmap:/build/littlenavmap" --volume "${PATH_TO_APROJECTS}/littlenavconnect:/build/littlenavconnect" --volume "${PATH_TO_APROJECTS}/aprojects/littlexpconnect:/build/littlexpconnect" --volume "${PATH_TO_APROJECTS}/atools:/build/atools" <image id>
```
