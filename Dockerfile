FROM debian:stretch-slim

LABEL maintainer="Alexandr Topilski <support@fastogt.com>"

ENV USER fastocloud
ENV APP_NAME fastocloud_load_balance
ENV PROJECT_DIR=/usr/src/fastocloud_load_balance
RUN groupadd -r $USER && useradd -r -g $USER $USER

COPY . /usr/src/fastocloud_load_balance

RUN set -ex; \
  BUILD_DEPS='ca-certificates git python3 python3-pip'; \
  PREFIX=/usr/local; \
  apt-get update; \
  apt-get install -y $BUILD_DEPS --no-install-recommends; \
  # rm -rf /var/lib/apt/lists/*; \
  \
  pip3 install setuptools; \
  PYFASTOGT_DIR=/usr/src/pyfastogt; \
  mkdir -p $PYFASTOGT_DIR && git clone https://github.com/fastogt/pyfastogt $PYFASTOGT_DIR && cd $PYFASTOGT_DIR && python3 setup.py install; \
  \
  cd $PROJECT_DIR/build && ./build_env.py --prefix=$PREFIX; \
  LICENSE_KEY="$(license_gen --machine-id)"; \
  cd $PROJECT_DIR/build && PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/usr/local/lib/pkgconfig python3 build.py release $LICENSE_KEY 1 $PREFIX; \
rm -rf $PYFASTOGT_DIR $PROJECT_DIR # && apt-get purge -y --auto-remove $BUILD_DEPS

COPY docker/fastocloud_load_balance.conf /etc/
RUN mkdir /var/run/$APP_NAME && chown $USER:$USER /var/run/$APP_NAME
VOLUME /var/run/$APP_NAME
WORKDIR /var/run/$APP_NAME

ENTRYPOINT ["fastocloud_load_balance"]

EXPOSE 5317 5001 6000
