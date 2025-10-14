import serial
import csv
import time
import os

CSV_FILE = r'C:\Users\Pocoyo\Desktop\Something-p3\Something-part2\Something.csv'
SERIAL_PORT = 'COM6'
BAUD_RATE = 9600

MENU = {
    '1': ("Fried Rice", 50),
    '2': ("Chicken Adobo", 100),
    '3': ("Pork Sisig", 150),
    '4': ("Beef Steak", 200),
    '5': ("Lechon Kawali", 250),
}

def get_user(uid):
    if not os.path.exists(CSV_FILE):
        return None
    with open(CSV_FILE, 'r', newline='') as csvfile:
        reader = csv.DictReader(csvfile)
        for row in reader:
            if row['UID'].strip().upper() == uid.strip().upper():
                return row
    return None

def display_menu():
    print("\n" + "="*40)
    print("           FOOD MENU")
    print("="*40)
    for num, (name, price) in MENU.items():
        print(f"{num}: {name} - {price} PHP")
    print("="*40)
    print("Instructions:")
    print("- Enter item number (1-5)")
    print("- For multiple quantities: '2x3' (3 Chicken Adobo)")
    print("- Type 'done' when finished ordering")
    print("="*40)

def add_to_order(selection, current_order, total_price):
    if 'x' in selection:
        try:
            item_num, qty_str = selection.split('x')
            item_num = item_num.strip()
            qty = int(qty_str.strip())
        except ValueError:
            print(f"Invalid format: {selection}")
            return False, current_order, total_price
    else:
        item_num = selection.strip()
        qty = 1
    if item_num in MENU and qty > 0:
        name, price = MENU[item_num]
        item_total = price * qty
        current_order.append({
            'name': name,
            'qty': qty,
            'price': price,
            'total': item_total
        })
        total_price += item_total
        print(f"Added: {qty}x {name} - {item_total} PHP")
        print(f"Current total: {total_price} PHP")
        return True, current_order, total_price
    else:
        print(f"Invalid item or quantity: {selection}")
        return False, current_order, total_price

def get_order_summary(current_order):
    detailed_items = []
    for item in current_order:
        detailed_items.append(f"{item['qty']}x {item['name']} ({item['total']}PHP)")
    return ', '.join(detailed_items)

def take_order():
    current_order = []
    total_price = 0
    display_menu()
    while True:
        selection = input("Enter your selection (or 'done' to finish): ").strip().lower()
        if selection == 'done':
            if current_order:
                print(f"\nOrder Summary:")
                for item in current_order:
                    print(f"  {item['qty']}x {item['name']} - {item['total']} PHP")
                print(f"Total: {total_price} PHP")
                return get_order_summary(current_order), total_price
            else:
                print("No items ordered. Please add items or type 'done' to cancel.")
        else:
            _, current_order, total_price = add_to_order(selection, current_order, total_price)

def update_account(uid, purchased_summary, total_price):
    rows = []
    found = False
    current_balance = 0
    user_name = ""
    response_code = "OK"

    if os.path.exists(CSV_FILE):
        with open(CSV_FILE, 'r', newline='') as csvfile:
            reader = csv.DictReader(csvfile)
            for row in reader:
                if row['UID'].lower() == uid.lower():
                    found = True
                    user_name = row['Name']
                    balance = int(row['Balance'])

                    if balance < total_price:
                        response_code = "REJECTED"
                        current_balance = balance
                        # Append row unchanged if insufficient balance
                        rows.append(row)
                    else:
                        new_balance = balance - total_price
                        row['Balance'] = str(new_balance)
                        current_balance = new_balance
                        rows.append(row)
                else:
                    rows.append(row)
    else:
        print(f"{CSV_FILE} not found. Creating new file.")
    
    if not found:
        response_code = "UNKNOWN"
        user_name = ""
        current_balance = 0
        print(f"!!! Unrecognized RFID UID: {uid} - Transaction rejected. !!!")

    if response_code == "OK":
        with open(CSV_FILE, 'w', newline='') as csvfile:
            fieldnames = ['UID', 'Name', 'Balance']
            writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(rows)

    print(f"\nTransaction Details:")
    print(f"UID: {uid}")
    print(f"User: {user_name}")
    print(f"Balance after purchase: {current_balance} PHP")
    print(f"Status: {response_code}")
    print(f"Purchased items: {purchased_summary}")
    print(f"Total price: {total_price} PHP")

    send_summary = purchased_summary
    if len(send_summary) > 100:
        send_summary = send_summary[:97] + "..."

    # Response for Arduino: RESPONSE_CODE,NAME,BALANCE,SUMMARY
    return f"{response_code},{user_name},{current_balance},{send_summary}\n"

def main():
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    except Exception as e:
        print(f"Cannot open serial port: {e}")
        return
    print("System ready. Waiting for Arduino...")
    
    current_uid = None  # Store the current UID
    
    while True:
        try:
            line = ser.readline().decode(errors='ignore').strip()
            if line.startswith("RFID_SCANNED:"):
                uid = line.replace("RFID_SCANNED:", "").upper()
                current_uid = uid  # Store the UID
                user = get_user(uid)
                if not user:
                    ser.write(b"UNKNOWN\n")
                    print(f"Unrecognized card: {uid}")
                    current_uid = None
                    continue
                else:
                    # Found - send OK, Name, and Balance
                    ser.write(f"OK,{user['Name']},{user['Balance']}\n".encode())
                    print(f"[CARD OK] {user['Name']} - Balance: {user['Balance']}")
            
            elif line == "MENU_READY":
                print("\nMenu Options:")
                print("1: Check Balance")
                print("2: Order")
                choice = input("Enter your choice (1 or 2): ").strip()
                ser.write(f"{choice}\n".encode())
                
                # Wait for Arduino's response
                menu_line = ser.readline().decode(errors='ignore').strip()
                if menu_line == "CHECK_BALANCE":
                    # Get fresh user data using stored UID
                    if current_uid:
                        user = get_user(current_uid)
                        if user:
                            ser.write(f"BALANCE:{user['Balance']}\n".encode())
                        else:
                            ser.write(b"ERROR\n")
                    else:
                        ser.write(b"ERROR\n")
                elif menu_line == "ORDER":
                    print("Order initiated.")
                    summary, total = take_order()
                    ser.write(f"SHOW_TOTAL:{total} PHP\n".encode())
                    # Wait for payment tap (Arduino will send same UID again)
                    uid_confirm = None
                    while True:
                        line2 = ser.readline().decode(errors='ignore').strip()
                        if line2.startswith("RFID_SCANNED:"):
                            uid_confirm = line2.replace("RFID_SCANNED:", "").upper()
                            break
                    # Validate card again and process payment
                    payment_result = update_account(uid_confirm, summary, total)
                    ser.write(payment_result.encode())
                    # Wait for transaction to complete/reset
                    while True:
                        end_line = ser.readline().decode(errors='ignore').strip()
                        if end_line == "TRANSACTION_COMPLETE":
                            ser.write(b"RESET_SYSTEM\n")
                            current_uid = None  # Reset for next transaction
                            break
                        
        except Exception as e:
            print(f"Error: {e}")
            time.sleep(1)

if __name__ == '__main__':
    # Test get_user function
    test_result = get_user("test_uid")
    print(test_result)
    # Then run main program
    main()
