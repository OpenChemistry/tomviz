from enum import Enum
from tomviz.state._pipeline import PipelineStateManager

class Palette(Enum):
    Current = ""
    TransparentBackground = "TransparentBackground"
    BlackBackground = "BlackBackground"
    DefaultBackground = "DefaultBackground"
    GradientBackground = "GradientBackground"
    GrayBackground = "GrayBackground"
    PrintBackground = "PrintBackground"
    WhiteBackground = "WhiteBackground"


#
# For now we will only support screenshoting the active view. In the future we
# can use a similar JSON patch syncing approach to support multiple views.
#
class View():
    def save_screenshot(self, file_path, palette=Palette.Current, width=-1, height=-1):
        PipelineStateManager().save_screenshot(file_path, palette.value, width, height)

def active_view():
    return View()