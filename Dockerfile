FROM gcc

ADD . /usr/src/statsd-tg/
WORKDIR /usr/src/statsd-tg/

RUN \
  aclocal && \
  autoheader && \
  automake --add-missing --copy && \
  autoconf && \
  ./configure && \
  make && \
  make install

ENTRYPOINT [ "statsd-tg" ]
CMD [ "-h" ]
