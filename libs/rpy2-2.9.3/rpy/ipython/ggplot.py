""" Goodies for ipython """

import os
import tempfile
import io
from rpy2 import robjects
from rpy2.robjects.packages import importr
from rpy2.robjects.lib import ggplot2, grdevices

from IPython.core.display import Image

# automatic plotting of ggplot2 figures in the notebook

class GGPlot(ggplot2.GGPlot):
    def png(self, width = 700, height = 500):
        """ Build an Ipython "Image" (requires iPython). """
        return image_png(self, width=width, height=height)

class GGPlotSVG(ggplot2.GGPlot):
    """ The embedding of several SVG figures into one ipython notebook is
    giving garbled figures. The SVG functionality is taken out to a
    child class.
    """
    def svg(self, width = 6, height = 4):
        """ Build an Ipython "Image" (requires iPython). """
        with grdevices.render_to_bytesio(grdevices.svg,
                                         width=width,
                                         height=height) as b:
            robjects.r("print")(self)
            data = b.getvalue()
            ip_img = Image(data=data, format='svg', embed=False)
            return ip_img

def image_png(gg, width=800, height=400):
    with grdevices.render_to_bytesio(grdevices.png,
                                     type="cairo-png", 
                                     width=width,
                                     height=height, 
                                     antialias="subpixel") as b:
        robjects.r("print")(gg)
    data = b.getvalue()
    ip_img = Image(data=data, format='png', embed=True)
    return ip_img

def display_png(gg, width=800, height=400):
    ip_img = image_png(gg, width=width, height=height)
    return ip_img._repr_png_()

def set_png_formatter():
    # register display func with PNG formatter:
    png_formatter = get_ipython().display_formatter.formatters['image/png']
    dpi = png_formatter.for_type(ggplot2.GGPlot, display_png)
    return dpi

class PNGplot(object):
    """
    Context manager
    """
    def __init__(self, width=600, height=400):
        self._width = width
        self._height = height
        png_formatter = get_ipython().display_formatter.formatters['image/png']
        self._png_formatter = png_formatter
        self._for_ggplot = self._png_formatter.for_type(ggplot2.GGPlot)
        
    def __enter__(self):
        self._png_formatter.for_type(ggplot2.GGPlot, display_png)
        return None

    def __exit__(self, exc_type, exc_val, exc_tb):
        self._png_formatter.for_type(ggplot2.GGPlot, self._for_ggplot)
        return False
