# [mrsh]

A minimal [POSIX] shell.

[![](https://builds.sr.ht/~emersion/mrsh.svg)](https://builds.sr.ht/~emersion/mrsh)

* POSIX compliant, no less, no more
* Simple, readable code without magic
* Library to build more elaborate shells

This project is a [work in progress].

## Build

Both Meson and POSIX make are supported. To use Meson:

    meson build
    ninja -C build
    build/mrsh

To use POSIX make:

    ./configure
    make
    ./mrsh

## Contributing

Either [send GitHub pull requests][GitHub] or [send patches on the mailing
list][ML].

## License

MIT

[mrsh]: https://mrsh.sh
[POSIX]: http://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html
[work in progress]: https://github.com/emersion/mrsh/issues/8
[GitHub]: https://github.com/emersion/mrsh
[ML]: https://lists.sr.ht/%7Eemersion/mrsh-dev
