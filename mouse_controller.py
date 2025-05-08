import pyautogui
import random
from pynput.mouse import Button, Controller

class MouseController:
    def __init__(self):
        self.mouse = Controller()
        self.screen_width, self.screen_height = pyautogui.size()

    def move_mouse(self, index_list):
        if len(index_list) >= 2:
            x = int(index_list[0] * self.screen_width)
            y = int(index_list[1] * self.screen_height)
            pyautogui.moveTo(x, y)

    """
    def move_mouse(self, index_finger_tip):
        if index_finger_tip is not None:
            x = int(index_finger_tip.x * self.screen_width)
            y = int(index_finger_tip.y / 2 * self.screen_height)
            pyautogui.moveTo(x, y)

    def left_click(self):
        self.mouse.press(Button.left)
        self.mouse.release(Button.left)

    def right_click(self):
        self.mouse.press(Button.right)
        self.mouse.release(Button.right)

    def double_click(self):
        pyautogui.doubleClick()

    def take_screenshot(self):
        im1 = pyautogui.screenshot()
        label = random.randint(1, 1000)
        im1.save(f'my_screenshot_{label}.png') 
    """