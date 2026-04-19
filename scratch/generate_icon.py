from PIL import Image
import os

# Original image: 7CR06003.jpg (4608x3072)
# Butterfly center is roughly at (2430, 1470)
# We crop a 600x600 area to encompass the butterfly and some background

cx, cy = 2430, 1470
size = 600
left = cx - size // 2
top = cy - size // 2
right = left + size
bottom = top + size

img = Image.open('7CR06003.jpg')
butterfly = img.crop((left, top, right, bottom))

# Save icons in various sizes
icon_sizes = [(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (16, 16)]
butterfly.save('app.ico', sizes=icon_sizes)

print("Successfully generated app.ico with multiple sizes.")
