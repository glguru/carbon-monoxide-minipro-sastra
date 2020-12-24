import gspread
from google.oauth2.service_account import Credentials
import time
import serial

scope = ['https://spreadsheets.google.com/feeds',
         'https://www.googleapis.com/auth/drive']

creds = Credentials.from_service_account_file('pro.json', scopes=scope)
client = gspread.authorize(creds)

data = client.open('MINI PRO').sheet1

arddat = serial.Serial("com7", 9600)
print("Connection established")

while True:
    a=(data.col_values(1))
    b=(data.col_values(2))
    
    for i in range(0, len(b)):
        b[i] = float(b[i])
        
        
    n= len(b)
    if b[n-1]>10:
        arddat.write(b'1')
    else:
        arddat.write(b'0')
    print("done")
    time.sleep(3)





