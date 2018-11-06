# mrsh

A minimal [POSIX][1] shell.

[![](https://builds.sr.ht/~emersion/mrsh.svg)](https://builds.sr.ht/~emersion/mrsh)

* POSIX compliant, no less, no more
* Simple, readable code without magic
* Library to build more elaborate shells

This project is a [work in progress](https://github.com/emersion/mrsh/issues/8).

## Build

```shell
meson build
ninja -C build
build/mrsh
```

## Contributing

Either [send GitHub pull requests][2] or [send patches on the mailing list][3].

## License

MIT

[1]: http://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html
[2]: https://github.com/emersion/mrsh
[3]: https://lists.sr.ht/%7Eemersion/mrsh-dev
