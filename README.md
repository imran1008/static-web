Static Web
===

What is it?
---
`static-web` is a template system that consumes a set of template HTML files as well as an input dataset and produces a finalized HTML document.

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

 - web-cc consumes a single template HTML file and produces a new HTML document with the extension .o.html . This HTML document is viewable in a browser but cross-page navigation may not work.
 - web-ld consumes all .o.html files and produces a new set of HTML documents in the specified output directory. The generated files will have a hashed filename that is based on the content. This will allow the web server to serve all HTML documents without a cache expiration time so they are always cached either in the proxies or the web browser.

Template syntax
---
The template syntax is non-intrusive such that a template HTML document should be viewable in a web browser.

The top of the HTML document should include comments that specify where to import data files. For example:

    `<!-- include table1.dat -->`
    `<!-- include table2.dat -->`

Variable substitution can be used anywhere in the document. For example:

    `<p> {{ title_name }} </p>`

To generate multiple copies of a set of elements, add the attribute `loop` with the value `{{ table_var }}` to the parent element. For example:

```
<div loop="{{ people }}">
	<div> {{ first_name }} </div>
	<div> {{ last_name }} </div>
</div>
```

