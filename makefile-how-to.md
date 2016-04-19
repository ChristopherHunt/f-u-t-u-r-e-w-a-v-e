Makefile How-To-Guide
=================

The project consists of a **master Makefile** at the top level directory of the
project. This master compiles all of the components of the project by
calling their individual Makefiles in turn. Additionally, the project itself has
a logical structure which should be maintained in order for the master Makefile
to properly build the project.

Also, since there is a new structure there is also some changes that will need
to be made to how you include files. Namely, you need to give an absolute path
from within the lib directory to the header you are including. So for instance,

    #include "MidiFile.h"  --needs-to-be-->  #include "midifile/include/MidiFile.h"

The reason for this is that the **master Makefile** only includes the `src/lib`
directory in the compilation process, so you have to give the rest of the path
to your header includes from there.

Where to put your code:
=================

The project is broken up as follows:

           src
      /     |     \
    lib    app    test

lib
------
The `lib` directory has directories for each of the library files needed for our
project. For instance, the `MidiFile.cpp` and `MidiFile.h` files are in the
`src/lib/midifile` directory. If you are building a set of source files which do
not have a main in them (aka they are just full of supporting functions), then
its probably a library and should go under `lib`.

app
------
The `app` directory has directories for each of the application files for our
project. If you are building a program which has a main then it is probably an
application and should go under the app directory. For instance, there is a
simple main that exercises some of the buffer functionality called
`midi_file_app.cpp`, and it is in the `src/app/midi_file_app` directory.

test
------
The `test` directoy has directories for each of the test applications for our
project. If you are building tests for part of the project then put your code in
here. Tests will be run at the beginning of each compile to ensure nothing has
broken them.

What to put in your code's directory
=================

Now, besides creating a directory for your code, you also need to put the proper
Makefile in the directory as well so it can be built correctly. This shouldn't
be too hard since you can always copy/paste a Makefile from a sibling directory
to get you started. Note that the Makefiles for libs, apps and tests are
different so you'll want to pull from the proper sibling directory for things to
work right. For instance, if you are making an app, you should copy the Makefile
in src/app/buffer_app into your directory.

Within each Makefile there will be a handful of things you need to change. Here
is a list of the things to worry about broken down by directory:

lib
------
    lib := name_of_your_library.a
    objs := your_source_file.o

app
------
    app := name_of_your_app_executable
    objs := your_source_file.o
    app_libs := a list of all of the archives which your application depends on.

For instance, the `midi_file_app` depends on `libmidifile.a` (for the midifile
functions).

test
------
    test := name_of_your_test_executable
    objs := your_source_file.o
    test_libs := a list of all of the archives which your test depends on.

Altering the Master Makefile
=================

Finally, you need to update the master Makefile so it knows that your source
code exists and to run it. Here is a breakdown of what to change for each type
of source (like above for the Makefiles).

lib
------
You need to let the Makefile know about your library. First, add a new entry
to the the Makefile listing your library. For instance, if you were intending on
building an archive called `blah.a` it would go in the master Makefile near
the rest of the library declarations (here's an example of the Makefile section
with it inserted):

    third_party_libs := src/lib
    libblah := src/lib/your_directory_name

Note that this is just a directory path from `src` to the directory holding your
project's code and its Makefile.

Once that is added, you need to append the library path to the list of libraries
to be built during compilation. So in the above example, you would append the
lib468 variable to the **libraries** variable as such:

    libraries := $(third_party_libs) $(libblah)

and you're done! Now during compilation your library will be built and placed in
the top-level `lib` folder for other programs to use.

app
------
You need to let the Makefile know about your application. First, add a new
entry to the master Makefile listing your app. For instance, if you were
intending on building an application called `hamsters_app` it would go in the
master Makefile near the rest of the library declarations (here's an example of
the Makefile section with it inserted):

    midi_file_app := src/app/midi_file_app
    hamster_app := src/app/hamster_directory_name

Note that this is just a directory path from `src` to the directory holding the 
hamster_app code and its Makefile.

Once that is added, you need to append the app path to the list of applications 
to be built during compilation. So in the above example, you would append the
hamster_app variable to the **apps** variable as such:

    apps := $(midi_file_app) $(hamster_app)

and you're done! Now once compilation is over you just need to look in the
top-level `bin` directory to find your application.

test
------
You need to let the Makefile know about your test. First, add a new entry to
the master Makefile listing your test. For instance, if you were intending on
building an application called `test_all_the_things` it would go in the master
Makefile near the rest of the library declarations (here's an example of the
Makefile section with it inserted):

    test_all_the_things := src/test/test_all_the_things

Note that this is just a directory path from `src` to the directory holding the 
test_all_the_things code and its Makefile.

Once that is added, you need to append the test path to the list of tests to be
built during compilation. So in the above example, you would append the
`test_all_the_things` variable to the **tests** variable as such:

    tests := $(test_all_the_things)

and you're done! Now during compilation your test will be run and the results
will be logged in the top-level `log` directory. Additionally, if an error
occurs it will be printed to screen during compilation. Finally, you can find
the executable for the test in the top-level `bin/test` directory for refernece.
