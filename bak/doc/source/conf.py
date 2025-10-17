import os
from datetime import datetime


project = "Modbus C Library"
author = "Luiz Carlos Gili"
copyright = f"{datetime.now():%Y}, {author}"
release = os.environ.get("MODBUS_DOC_VERSION", "1.0.0")

extensions = [
    "breathe",
    "sphinx.ext.autodoc",
    "sphinx.ext.autosummary",
    "sphinx.ext.autosectionlabel",
    "sphinx.ext.todo",
    "sphinx.ext.viewcode",
    "sphinx.ext.napoleon",
    "sphinx.ext.intersphinx",
]

templates_path = ["_templates"]
exclude_patterns = []

autosectionlabel_prefix_document = True
autosummary_generate = True
todo_include_todos = True

doxygen_xml_dir = os.environ.get("DOXYGEN_XML_DIR", os.path.abspath(os.path.join("..", "..", "build", "doxygen", "xml")))
breathe_projects = {"modbus": doxygen_xml_dir}
breathe_default_project = "modbus"

html_theme = "sphinx_rtd_theme"
static_dir = os.path.join(os.path.dirname(__file__), "_static")
html_static_path = ["_static"] if os.path.isdir(static_dir) else []
html_theme_options = {
    "navigation_depth": 3,
    "collapse_navigation": False,
}

intersphinx_mapping = {
    "python": ("https://docs.python.org/3", None),
}
