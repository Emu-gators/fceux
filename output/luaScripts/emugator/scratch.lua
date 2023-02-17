require("wx")
-- define some constants for the rectangle
local RECT_WIDTH = 50
local RECT_HEIGHT = 50
local RECT_SPEED = 5

-- define a variable to track the position of the rectangle
local rectX = 0
local rectY = 0

-- define the OnPaint function to draw the rectangle
local function OnPaint(event)
  local dc = wx.wxPaintDC(event:GetEventObject())
  dc:Clear() -- clear the drawing area

  -- draw the rectangle at its current position
  dc:SetBrush(wx.wxBLACK_BRUSH)
  dc:SetPen(wx.wxTRANSPARENT_PEN)
  dc:DrawRectangle(rectX, rectY, RECT_WIDTH, RECT_HEIGHT)
end

-- define a timer function to update the position of the rectangle
local function OnTimer()
  rectX = rectX + RECT_SPEED
  rectY = rectY + RECT_SPEED

  -- invalidate the frame to trigger a redraw
  local frame = wx.wxGetApp():GetTopWindow()
  frame:Refresh()
end

-- create the frame and start the animation
local frame = wx.wxFrame(wx.NULL, wx.wxID_ANY, "Animation", wx.wxDefaultPosition, wx.wxSize(800, 600))
frame:Connect(wx.wxEVT_PAINT, OnPaint)
frame:Show()

local timer = wx.wxTimer(frame)
frame:Connect(wx.wxEVT_TIMER, function(event) OnTimer() end)
timer:Start(50, wx.wxTIMER_CONTINUOUS)

wx.wxGetApp():MainLoop()