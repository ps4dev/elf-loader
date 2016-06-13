# ps4dev/elf-loader
#
# FIXME: Commit based docker hub webhooks are not a good trigger for dependent repos
# The build is standalone for now (duplicates the sdk build, remove resources)
# Need to change trigger or use travis for a more controllable build

FROM voidlinux/voidlinux

ENV PS4DEV=/home/ps4dev
ENV PS4SDK=$PS4DEV/ps4sdk

RUN \
    groupadd -r ps4dev && \
    useradd --create-home -d /home/ps4dev --gid ps4dev ps4dev

WORKDIR $PS4DEV
COPY . $PS4DEV/elf-loader

RUN \
    xbps-install -Sy xbps ; \
    xbps-install -Sy make clang git nodejs && \
    git clone https://github.com/ps4dev/ps4sdk.git && \
    cd ps4sdk && \
    make && \
    cd ../elf-loader && \
    make && \
    cd .. && \
    rm -R /home/ps4dev/ps4sdk && \
    mv /home/ps4dev/elf-loader/local /home/ps4dev/elf-loader_ && \
    rm -R /home/ps4dev/elf-loader/* && \
    mv /home/ps4dev/elf-loader_ /home/ps4dev/elf-loader/local && \
    chown -R ps4dev:ps4dev /home/ps4dev ; \
    xbps-remove -Ry make clang git ; \
    xbps-remove -Oy ; \
    xbps-remove -oy

USER ps4dev

EXPOSE 5350

WORKDIR $PS4DEV/elf-loader/local

ENTRYPOINT ["node", "server.js", "."]
CMD ["5350"]
