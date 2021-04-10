Static Web
===

What is it?
---
[static-web](https://gitea.cubicsignal.com/imran/static-web) is a template system that consumes a set of template HTML files as well as an input dataset and produces a set of finalized HTML documents.

Project status
---
This project is under heavy development and is in its very early stage. It is nowhere near function complete.

Motivation
---
Most, if not all, modern web frameworks are very dynamic. They are completely driven by JavaScript and they require a running database(s) and/or service(s) for even the most basic functionality. The dynamic nature of these frameworks give it a lot of flexibility but it also introduces fragility to the system. Here is a list of disadvantages of a dynamic web site:

 - Multiple database and service calls for every site visit, even when the output is the same
 - Strains the web server because of repeated computation for every site visit
 - Requires the developer to accurately guess the expiration of cached content
 - Testing a web site requires the use of a real web server
 - Requires transactional database copy to backup the site contents

For web sites that are frequently read from but infrequently written to, the dynamic web frameworks provide very little benefit but still introduces all the drawbacks mentioned above.

Project description
---
`static-web` provides a set of tools that allow the developer to process template HTML files and produce finalized HTML documents.

 - `web-cc` consumes a single template HTML file and produces a set of HTML documents inside a new directory with the suffix `.o` . These HTML documents are viewable in a browser but cross-page navigation may not work.
 - `web-ld` consumes a set of `.o` directories and produces a new set of HTML documents in the specified output directory. The generated files will have a hashed filename that is based on the content. This will allow the web server to serve all HTML documents without a cache expiration time so they are always cached either in the proxies or the web browser. The output directory will also contain HTML documents with human-readable filenames that will redirect the browser to the real document with the hashed filename. This redirection file can be used for bookmarking purposes or shareable links.

Implementation design goal
---
This project is designed with the principles of data-oriented design. A lot of emphasis is given to the implementation to improve optimizability by the compiler and to reduce as much cache miss rate as possible.

Template syntax
---
The template syntax is non-intrusive such that a template HTML document should be viewable in a web browser.

The top-level html tag should contain the `include` attribute that contains a space-separated list of includes.  For example:

```
<html include="table1.md table2.md">
</html>
```

Variable substitution can be used anywhere in the document. For example:

    `<p> {{ title_name }} </p>`

To generate multiple copies of a set of elements, add the attribute `data` with the value `table_var` to the parent element. For example:

```
<html include="table1.dat table2.dat" data="main_table">
	<div data="people">
		<div> {{ first_name }} </div>
		<div> {{ last_name }} </div>
	</div>
</html>
```

The `data` attribute in the `<html>` tag has a special meaning. `web-cc` will generate a new document inside the `.o` directory for each row of this table.

Data syntax
---
The input data for the templates will be stored as markdown documents. Most git web frontends provide a nice markdown viewer and editor. Using the markdown format will enable developers to quickly make hand-written edits when necessary.

Build instructions
---
This project requires the [meson](https://mesonbuild.com/) build system. Run the following commands to build all targets:

```
meson src build
ninja -C build
```


