# Rules for sox_ng issues

## Usage

The master copy of the issues lives on Codeberg.

You can make a local copy of them into the `issues` subdirectory
by going
```
sh issues/getissues.sh
```
which fetches each issue into an `.md` file named according
to the issue's title, and its metadata and attachments into
a directory of the same name without the `.md` suffix.

If you migrate the Codeberg repository to another Forgejo instance,
attachments to issues do not make it
[forgejo issue 4787](https://codeberg.org/forgejo/forgejo/issues/4787)
but if `pincopallino` has made a migration on codeberg.org,
they can be restored from the copy made above by going:
```
issues/putissues.sh codeberg.org/pincopallino/sox_ng -f
```
where `-f` means "don't worry if the issue `id`s don't match."

Like the wiki, it has a script `makehtml.sh` to make HTML pages of the issues.

In future, the master copy of the issue database will live in the
source repository and the web version will be a copy of it (#80)
but for the moment the preferred way to edit the issues is via the
web interface.

It is possible to make a new issue from the command line by creating
`issues/"Don't worry, be happy".md` and in a directory
`issues/"Don't worry, be happy"` placing files `milestone`
and maybe `labels` and then saying
`issues/putissues.sh` using your Codeberg username and password.
It will fill in the `number` file with whatever forgejo assigns to it.

## Format of an issue

### Title

A one-liner, as short as possible.

For the issue downloader to work on Windows and MS/DOS,
you should avoid slash, backslash, colon and double quotes.

If a new issue has the same cause as an existing one,
its title (not its first line) should end `=#35`
so that the summary of issues says where to go for the best info.

### Description

The first line of every issue is `# Title`, the same as the Title.

The second-level headings are usually
`## Links`, `## Description`, `## Repeat by`, `## Results`,
`## Analysis` and `## Conclusion`.

Commit hashes should be cited as their first seven digits because
that makes it easier to search for them with precision.

In the content, [semantic line breaks](https://sembr.org)
and less-than-80-column lines are preferred to long lines
so that the `.md` version is more readable.  
Unfortunately, `forgejo` renders all line breaks in its pages
for the issues even though it doesn't do this to the wiki.

[The Markdown Guide](https://www.markdownguide.org/basic-syntax/#line-breaks)
recommends ending a line with two spaces or <BR>
to get an explicit line break. `forgejo` viewing an
`.md` file in the source tree doesn't honour double spaces,
but the `forgejo` wiki and `makehtml.sh` (i.e. `multimarkdown`) do.

For further info on the Markdown used in issues and the wiki
see RULES-issues.md

### Attachments

Test files (small ones only please!) and patches
need to be attached to the main description.

### Comments

Issue comments are not downloaded.

If people add them on the web version, other webby people can edit
the wisdom in them into the main description; that way our
command-line friends get the best version of the problem description
and are spared wading through the chitchat in search of gems.

### Milestone

All issues should have a milestone, one of:
* `micro` for bug fixes
* `minor` for enhancements
* `major` for non backward-compatible changes
* `release` if it regards the SoX_ng project's infrastructure

Milestones `micro` or `minor` are used instead of
the conventional labels `bug` or `enhancement`.

### Labels

All optional:
* `bounty`: Someone has offered money to whoever resolves this issue
* `copyright`: The issue impacts on `sox_ng`'s copyright status
* `duplicate`: This report has the same cause as another issue; go there
* `invalid`: This reported bug does not affect `sox_ng`
* `needswork`: All info seems to be in, but work is needed
* `patch`: A solution is available, maybe as an attached patch
* `unconfirmed`: We have heard of a bug but not yet seen whether it bites us
