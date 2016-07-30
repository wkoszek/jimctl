# JimTcl fixed to work on BSD, Linux and Windows

This is a [JimTcl](http://jim.tcl.tk/index.html/doc/www/www/index.html)
version from circa before 2010. I took it since the build was entirely
`makefile` based and it was easy to port JimTcl back then. I made it work on
3 platforms: FreeBSD, GNU/Linux and Windows. For the invonvenience of not
having all bugs and features from 2010-... you get something that is easier
to port. The original README is [here](README)

This is a copy of my original repo on
[http://repo.or.cz/jimtcl/wkoszek.git](http://repo.or.cz/jimtcl/wkoszek.git).
Since I no longer use `repo.or.cz` feel free to contribute only to GitHub
version, if you're interested.

# Notes

Some random notes extracted from
http://www.sqlite.org:8080/pipermail/jim-devel/2011-August/000589.html
email:

```
You can get JimTcl installed on Windows.

If you have MinGW, you should be able to create .dll. I think
this .dll should be acceptable to use with VC++ programs. In other
words: if you know how to load .dll from your program compiled with
VC++, you shouldn't have bigger problems.

Synchronize to my older JimTcl branch (which is known to compile and
work on FreeBSD, GNU/Linux, Solaris and Windows [Cygwin/MinGW]):

	git clone http://repo.or.cz/r/jimtcl/wkoszek.git

The only thing which is necessary to make it work after that is:

	make jim

And if you want library, just do:

	make libjib

For making it compile with VC++ it would be a bit more work, but
not too much.

If you provide me clean enough patch / exact and detailed changes to
make it work, I'll incorporate your changes in my branch.
```
