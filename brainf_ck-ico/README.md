# Brainf_ck-ico

This is an implementation in Ico of the language [Brainf_ck](https://brainfuck.org) (I prefer to censor the name). The implementation is in the form of an interactive interpreter. It cannot run source files, as Ico does not have support for reading from files. The features are also incomplete, because currently Ico does not support escaped characters, such as the newline character.

Example:

```
$ build/ico brainf_ck-ico/bf.ic
bf> >+++++++++[<++++++++>-]<.>+++++++[<++++>-]<+.+++++++..+++.[-]>++++++++[<++++>-] <.>+++++++++++[<++++++++>-]<-.--------.+++.------.--------.[-]>++++++++[<++++>- ]<+.[-]++++++++++.
Hello world!
```