import serial
import time
import schedule 
import gspread
from google.oauth2.service_account import Credentials

scope = ['https://spreadsheets.google.com/feeds',
         'https://www.googleapis.com/auth/drive']

creds = Credentials.from_service_account_file('pro.json', scopes=scope)
client = gspread.authorize(creds)

data = client.open('MINI PRO')
worksheet = data.sheet1

arduino = serial.Serial("com5", 9600)
print("Connection established.")
count=1
while True:
    dat = arduino.readline()

    listinfloat=list()

    dec_data = str(dat[0:len(dat)].decode("utf-8"))
    if 'x' in dec_data:
        dat_ls = dec_data.split('x')
        for item in dat_ls:
            item.strip()
            listinfloat.append(float(item))

        print("Data", listinfloat)
        count=count+1
        if count%5==0:
            worksheet.update_cell(count/5,1,count/5)
            worksheet.update_cell(count/5,2,listinfloat[2])
            print("uploaded ",count/5)
    else:
        dat_ls = dec_data.split(" ")
        print("Collected data", dat_ls)
    dat = 0
    listinfloat.clear()
    dat_ls.clear()



