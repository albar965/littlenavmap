## Contributing

Thank you for contributing to _Little Navmap_! Your help is very welcome!

Please note that this project is released with a [Contributor Code of Conduct][code-of-conduct]. By participating in this project you agree to abide by its terms.

## Branches

Make sure to clone or fork the right branch before adding changes or issuing a pull request.

- `master` is the unstable branch where all the development is going on. All versions have odd minor numbers like 1.**3**.0.develop or 1.**1**.8.develop.
- `release/X.X` Is a release branch containing betas, release candidates and the tagged stable releases. Versions have even minor numbers like 1.**4**.0.beta or 1.**6**.3.rc1.
- `feature/translation` A feature branch containing unfinished translations which are cherry picked into `master` from time to time.
- `feature/YOURFEATURE` A feature branch for separate development.

Older release branches are deleted after creating a tag `archive/release/X.X` on the revision.

## Submitting a pull request

0. [Fork][fork] and clone the repository. You have to clone at least the modified [Marble Repositiory][marble] and my library [atools][atools]. The build process is quite complex on any other than Linux platform.
0. Create a new branch: `git checkout -b feature/my-feature-name`
0. Push to your fork and [submit a pull request][pr]
0. Wait for your pull request to be reviewed and merged.

Here are a few things you can do that will increase the likelihood of your pull request being accepted:

- Follow the style and format your code according to the included `uncrustify.cfg` configuration files. Get _Uncrustify_ [here](http://uncrustify.sourceforge.net/). Avoid formatting any other code than the one you changed.
- Keep your change as focused as possible. If there are multiple changes you would like to make that are not dependent upon each other, consider submitting them as separate pull requests.
- Write a [good commit message](http://tbaggery.com/2008/04/19/a-note-about-git-commit-messages.html).

## Resources

- [Contributing to Open Source on GitHub](https://guides.github.com/activities/contributing-to-open-source/)
- [Using Pull Requests](https://help.github.com/articles/using-pull-requests/)
- [GitHub Help](https://help.github.com)

[atools]: https://github.com/albar965/atools
[marble]: https://github.com/albar965/marble
[fork]: https://github.com/albar965/littlenavmap/fork
[pr]: https://github.com/albar965/littlenavmap/compare
[code-of-conduct]: CODE_OF_CONDUCT.md
