from PIL import Image
import os

img = Image.open('7CR06003.jpg')
w, h = img.size

# Trying to find the butterfly. 
# Looking at the image, it's roughly 50% across and 45% down.
# Butterfly is small.
# Center: 2304, 1536
# Let's crop a larger area first to find it.
box = (2000, 1000, 3000, 2000)
crop = img.crop(box)
crop.save('search_crop.jpg')
print(f"Saved search_crop.jpg with size {crop.size}")
