import jinja2
from IPython.display import HTML


template_list = jinja2.Template("""
<p><emph>{{ clsname }}</emph> with {{ rlist | length }} elements:</p>
<dl class="rpy2">
{%- for elt_i in range(display_neltmax) %}
      <dt>{{ rlist.names[elt_i] }}</dt>
      <dd>{{ rlist[elt_i] }}</dd>
{%- endfor %}
{%- if display_neltmax < (rlist | length) %}
      <dt>...</dt>
      <dd></dd>
{%- endif %}
</dl>
""")

template_vector_horizontal = jinja2.Template("""
<emph>{{ clsname }}</emph> with {{ vector | length }} elements:
<table class="{{ table_class }}">
<thead>
</thead>
<tbody>
<tr>
  {%- for elt_i in range(display_ncolmax - size_tail) %}
  <td>{{ vector[elt_i] }}</td>
  {%- endfor %}
  {%- if display_ncolmax < (vector | length) %}
  <td>...</td>  
  {%- endif %}
  {%- for elt_i in elt_i_tail %}
      <td>{{ vector[elt_i] }}</td>
  {%- endfor %}
</tr>
</tbody>
</table>
""")

template_vector_vertical = jinja2.Template("""
<emph>{{ clsname }}</emph> with {{ vector | length }} elements:
<table class="{{ table_class }}">
<thead>
</thead>
<tbody>
  {%- for elt_i in range(display_nrowmax - size_tail) %}
  <tr>
    <td class="rpy2_rowname">{{ elt_i }}</td>
  {%- if has_vector_names %}
    <td class="rpy2_names">{{ vector.names[elt_i] }}</td>
  {%- endif %}
    <td>{{ vector[elt_i]}}</td>
  </tr>
  {%- endfor %}
  {%- if display_nrowmax < (vector | length) %}
  <tr>
    <td class="rpy2_rowname">...</td>
  {%- if has_vector_names %}
    <td class="rpy2_names">...</td>
  {%- endif %}
    <td>...</td>  
  </tr>
  {%- endif %}
  {%- for elt_i in elt_i_tail %}
  <tr>
    <td class="rpy2_rowname">{{ elt_i }}</td>
  {%- if has_vector_names %}
    <td class="rpy2_names">{{ vector.names[elt_i] }}</td>
  {%- endif %}
    <td>{{ vector[elt_i] }}</td>
  </tr>
  {%- endfor %}
</tr>
</tbody>
</table>
""")

template_dataframe = jinja2.Template("""
<emph>{{ clsname }}</emph> with {{ dataf.nrow }} rows and {{ dataf | length }} columns:
<table class="{{ table_class }}">
  <thead>
    <tr class="rpy2_names">
      <th></th>
      {%- if has_rownames %}
      <th></th>
      {%- endif %}

{%- for col_i in range(display_ncolmax - size_coltail) %}
      <th>{{ dataf.names[col_i] }}</th>
{%- endfor %}
{%- if display_ncolmax < dataf.ncol %}
      <th>...</th>  
{%- endif %}
{%- for col_i in col_i_tail %}
      <th>{{ dataf.names[col_i] }}</th>
{%- endfor %}
    </tr>
  </thead>
  <tbody>
{%- for row_i in range(display_nrowmax - size_rowtail) %}
    <tr>
      <td class="rpy2_rowname">{{ row_i }}</td>
      {%- if has_rownames %}
        <td class="rpy2_names">{{ dataf.rownames[row_i] }}</td>
      {%- endif %}
  {%- for col_i in range(display_ncolmax - size_coltail) %}
      <td>{{ dataf[col_i][row_i] }}</td>
  {%- endfor %}
  {%- if display_ncolmax < dataf.ncol %}
       <td>...</td>  
  {%- endif %}
  {%- for col_i in col_i_tail %}
      <td>{{ dataf[col_i][row_i] }}</td>
  {%- endfor %}
    </tr>
{%- endfor %}

{%- if dataf.nrow > display_nrowmax %}
    <tr>
      <td class="rpy2_rowname">...</td>
      {%- if has_rownames %}
        <td class="rpy2_names">...</td>
      {%- endif %}

  {%- for col_i in range(display_ncolmax - size_coltail) %}
      <td>...</td>
  {%- endfor %}
  {%- if display_ncolmax < dataf.ncol %}
       <td>...</td>  
  {%- endif %}
  {%- for col_i in range(2) %}
      <td>...</td>
  {%- endfor %}
    </tr>
{%- endif %}

{%- for row_i in row_i_tail %}
    <tr>
      <td class="rpy2_rowname">{{ row_i }}</td>
      {%- if has_rownames %}
        <td class="rpy2_names">{{ dataf.rownames[row_i] }}</td>
      {%- endif %}
  {%- for col_i in range(display_ncolmax - size_coltail) %}
      <td>{{ dataf[col_i][row_i] }}</td>
  {%- endfor %}
  {%- if display_ncolmax < dataf.ncol %}
       <td>...</td>  
  {%- endif %}
  {%- for col_i in col_i_tail %}
      <td>{{ dataf[col_i][row_i] }}</td>
  {%- endfor %}
    </tr>
{%- endfor %}
  </tbody>
</table>
""")

template_ridentifiedobject = jinja2.Template("""
<ul style="list-style-type: none;">
<li>{{ clsname }} object</li>
<li>Origin in R: {{ origin }}</li>
<li>Class(es) in R:
  <ul>
  {%- for rclsname in obj.rclass %}
    <li>{{ rclsname }}</li>
  {%- endfor %}
  </ul>
</li>
</ul>
""")

template_rs4 = jinja2.Template("""
<table class="{{ table_class }}">
<tr>
  <th colspan="2">{{ clsname }} object</th>
</tr>
<tr>
  <th>Origin in R:</th>
  <td> {{ origin }}</td>
</tr>
<tr>
  <th>Class(es) in R:</th>
  <td>
    <ul>
  {%- for rclsname in obj.rclass %}
      <li>{{ rclsname }}</li>
  {%- endfor %}
    </ul>
  </td>
</tr>
<tr>
  <th>Attributes:</th>
  <td>
  <ul>
  {%- for sln in obj.slotnames() %}
    <li>{{ sln }}</li>
  {%- endfor %}
  </ul>
  </td>
</tr>
</table>
""")

template_sourcecode = jinja2.Template("""
<p>
R source code:
</p>
<style>
{{ syntax_highlighting }}
</style>
{{ sourcecode }}
""")

from rpy2.robjects import (vectors, 
                           RObject, 
                           SignatureTranslatedFunction,
                           RS4,
                           SourceCode)


class StrFactorVector(vectors.FactorVector):

    def __getitem__(self, item):
        integer = super(StrFactorVector, self).__getitem__(item)
        # R is one-offset, Python is zero-offset
        return self.levels[integer-1]

class StrDataFrame(vectors.DataFrame):
    
    def __getitem__(self, item):
        obj = super(StrDataFrame, self).__getitem__(item)
        if isinstance(obj, vectors.FactorVector):
            obj = StrFactorVector(obj)
        return obj


def html_vector_horizontal(vector,
                           display_ncolmax=10,
                           size_tail=2,
                           table_class='rpy2_table'):
    if isinstance(vector, vectors.FactorVector):
        vector = StrFactorVector(vector)
    html = template_vector_horizontal.render({
        'table_class': table_class,
        'clsname': type(vector).__name__,
        'vector': vector,
        'display_ncolmax': min(display_ncolmax, len(vector)),
        'size_tail': size_tail,
        'elt_i_tail': range(max(0, len(vector)-size_tail), len(vector))})
    return html

def html_rlist(vector,
               display_nrowmax=10,
               size_tail=2,
               table_class='rpy2_table'):
    html = template_vector_vertical.render({
        'table_class': table_class,
        'clsname': type(vector).__name__,
        'vector': vector,
        'has_vector_names': vector.names is not rinterface.NULL,
        'display_nrowmax': min(display_nrowmax, len(vector)),
        'size_tail': size_tail,
        'elt_i_tail': range(max(0, len(vector)-size_tail), len(vector))})
    return html

def html_rdataframe(dataf,
                    display_nrowmax=10,
                    display_ncolmax=6,
                    size_coltail=2, size_rowtail=2,
                    table_class='rpy2_table'):
    html = template_dataframe.render(
        {'dataf': StrDataFrame(dataf),
         'table_class': table_class,
         'has_rownames': dataf.rownames is not None,
         'clsname': type(dataf).__name__,
         'display_nrowmax': min(display_nrowmax, dataf.nrow),
         'display_ncolmax': min(display_ncolmax, dataf.ncol),
         'col_i_tail': range(max(0, dataf.ncol-size_coltail), dataf.ncol),
         'row_i_tail': range(max(0, dataf.nrow-size_rowtail), dataf.nrow),
         'size_coltail': size_coltail,
         'size_rowtail': size_rowtail
     })
    return html

def html_sourcecode(sourcecode):
    from pygments import highlight
    from pygments.lexers import SLexer
    from pygments.formatters import HtmlFormatter
    formatter = HtmlFormatter()
    htmlcode = highlight(sourcecode, SLexer(), formatter)
    d = {'sourcecode': htmlcode,
         'syntax_highlighting': formatter.get_style_defs()}
    html = template_sourcecode.render(d)
    return html

# FIXME: wherefrom() is taken from the rpy2 documentation
# May be it should become part of the rpy2 API
from rpy2 import rinterface
def wherefrom(name, startenv=rinterface.globalenv):
    """ when calling 'get', where the R object is coming from. """
    env = startenv
    obj = None
    retry = True
    while retry:
        try:
            obj = env[name]
            retry = False
        except LookupError:
            env = env.enclos()
            if env.rsame(rinterface.emptyenv):
                retry = False
            else:
                retry = True
    return env

def _dict_ridentifiedobject(obj):
    if hasattr(obj, '__rname__') and obj.__rname__ is not None:
        env = wherefrom(obj.__rname__)
        try:
            origin = env.do_slot('name')[0]
        except LookupError:
            origin = 'package:base ?'
    else:
        origin = '???'
    d = {'clsname': type(obj).__name__,
         'origin': origin,
         'obj': obj}
    return d

def html_ridentifiedobject(obj):
    d = _dict_ridentifiedobject(obj)
    html = template_ridentifiedobject.render(d)
    return html

def html_rs4(obj, table_class='rpy2_table'):
    d = _dict_ridentifiedobject(obj)
    d['table_class']=table_class
    html = template_rs4.render(d)
    return html

def init_printing():
    ip = get_ipython()
    html_f = ip.display_formatter.formatters['text/html']
    html_f.for_type(vectors.Vector, html_vector_horizontal)
    html_f.for_type(vectors.ListVector, html_rlist)
    html_f.for_type(vectors.DataFrame, html_rdataframe)
    html_f.for_type(RObject, html_ridentifiedobject)
    html_f.for_type(RS4, html_rs4)
    html_f.for_type(SignatureTranslatedFunction, html_ridentifiedobject)
    html_f.for_type(SourceCode, html_sourcecode)

