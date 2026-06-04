# Keep ofxImGui on openFrameworks event listeners for this example. The GLFW
# callback replacement path can crash during Gui::setup() with this OF build.
PROJECT_CFLAGS = -DOFXIMGUI_GLFW_EVENTS_REPLACE_OF_CALLBACKS=0
