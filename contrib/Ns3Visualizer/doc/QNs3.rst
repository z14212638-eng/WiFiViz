Example Module Documentation
============================

.. include:: replace.txt
.. highlight:: cpp

.. heading hierarchy:
   ============= Module Name
   ------------- Section (#.#)
   ~~~~~~~~~~~~~ Subsection (#.#.#)

This is the outline for adding new module documentation to |ns3|.
See ``src/lr-wpan/doc/lr-wpan.rst`` for an example.

This first part is for describing what the model is trying to
accomplish. General descriptions and design, overview of the project
goes in this part without the need of any subsections. A visual summary
is also recommended.

For consistency (italicized formatting), please use |ns3| to refer to
ns-3 in the documentation (and likewise, |ns2| for ns-2).  These macros
are defined in the file ``replace.txt``.

Detailed |ns3| Sphinx documentation guidelines can be found `here <https://www.nsnam.org/docs/contributing/html/models.html>`_


The source code for the new module lives in the directory ``contrib/QNs3``.


Scope and Limitations
---------------------

This is should be your first section. Please use it to list the scope and
limitations. What can the model do?  What can it not do? Be brief and concise.

Section A
---------

Free form. Your second section of your documentation and its subsections goes here.
The documentation can contain any number of sections but be careful to not use
more than two levels in subsections.

Section B
---------

Free form. The last section of your documentation with content goes here.

Usage
-----

A brief description of the module usage goes here. This section must be present.

Helpers
~~~~~~~

A subsection with a description of the helpers used by the model goes here. Snippets of code are
preferable when describing the helpers usage. This subsection must be present.
If the model CANNOT provide helpers write "Not applicable".

Attributes
~~~~~~~~~~

A subsection with a list of attributes used by the module, each attribute should include a small
description. This subsection must be present.
If the model CANNOT provide attributes write "Not applicable".

Traces
~~~~~~

A subsection with a list of the source traces used by the module, each trace should include a small
description. This subsection must be present.
If the model CANNOT provide traces write "Not applicable".

Examples and Tests
------------------

A brief description of each example and test present in the model must be here.
Include both the name of the example file and a brief description of the example.
This section must be present.

Validation
----------

Describe how the model has been tested/validated.  What tests run in the
test suite?  How much API and code is covered by the tests?

This section must be present. Write ``No formal validation has been made`` if your
model do not contain validations.

References
----------

The reference material used in the construction of the model.
This section must be the last section and must be present.
Use numbers for the index not names when listing the references. When possible, include the link of the referenced material.

Example:

[`1 <https://ieeexplore.ieee.org/document/6012487>`_] IEEE Standard for Local and metropolitan area networks--Part 15.4: Low-Rate Wireless Personal Area Networks (LR-WPANs)," in IEEE Std 802.15.4-2011 (Revision of IEEE Std 802.15.4-2006) , vol., no., pp.1-314, 5 Sept. 2011, doi: 10.1109/IEEESTD.2011.6012487.

