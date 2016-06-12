# ps4dev/elf-loader
# FIXME: Bad for webhooks (make standalone - git clone sdk here)

FROM ps4dev/elf-loader

WORKDIR $PS4DEV/elf-loader

COPY . $PS4DEV/elf-loader

RUN xbps-install -Su xbps && xbps-install nodejs && make

EXPOSE 5350

ENTRYPOINT ["node", "$PS4DEV/elf-loader/local/server.js"]

VOLUME $PS4DEV/elf-loader
