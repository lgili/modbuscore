# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

import os
import sys
sys.path.insert(0, os.path.abspath('../..'))  # Ajuste o caminho conforme necessário


project = 'modbus'
copyright = '2025, Luiz Carlos Gili'
author = 'Luiz Carlos Gili'
release = '0.1v'

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = [
    'breathe',
    'sphinx.ext.autodoc',
    'sphinx.ext.napoleon',
    'sphinx.ext.viewcode',
]

templates_path = ['_templates']
exclude_patterns = []

# Configuração do Breathe
breathe_projects = {
    "Modbus": "xml"
}
breathe_default_project = "Modbus"

import subprocess
doxygen_output = os.path.join(os.path.abspath('../..'), 'doc', 'xml')
breathe_projects["Modbus"] = doxygen_output

# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = 'sphinx_rtd_theme'
html_static_path = ['_static']
