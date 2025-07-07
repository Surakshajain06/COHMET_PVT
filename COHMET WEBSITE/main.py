from gtts import gTTS

# Your text
text = "Please "

# Language (en = English)
language = 'en'

# Create gTTS object
tts = gTTS(text=text, lang=language)

# Save to MP3 file
tts.save("output.mp3")

print("MP3 file saved as output.mp3")
