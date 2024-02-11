
import socket
import threading
import time
from tkinter import *
from tkinter import scrolledtext

class ClientGUI:
    def __init__(self, master):

        self.is_updating_from_server = False  # Flag to track source of updates

        self.master = master
        master.title("Client")

        self.host = "127.0.0.1"  # Default server address
        self.port = 12345        # Server's port
        self.clientSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

        # Configure the grid layout manager
        master.columnconfigure(1, weight=1)
        master.rowconfigure(4, weight=1)  # Give more weight to the content row

        # Server Address Entry
        self.server_address_label = Label(master, text="Server Address:")
        self.server_address_label.grid(row=0, column=1, sticky=E)
        self.server_address_entry = Entry(master)
        self.server_address_entry.insert(0, self.host)  # Default value
        self.server_address_entry.grid(row=0, column=2, sticky='ew')

        # Connect button
        self.connect_button = Button(master, text="Connect", command=self.connect)
        self.connect_button.grid(row=0, column=0, padx=5, pady=5, sticky='ew')

        # Create file label and entry
        self.create_file_label = Label(master, text="Filename:")
        self.create_file_label.grid(row=1, column=1, sticky=W)
        self.create_file_entry = Entry(master)
        self.create_file_entry.grid(row=1, column=2, sticky='ew')
        # Create File button
        self.create_button = Button(master, text="Create File", command=self.create_file)
        self.create_button.grid(row=1, column=0, padx=5, pady=5, sticky='ew')

        # Edit Filename label and entry
        self.edit_file_label = Label(master, text="Edit Filename:")
        self.edit_file_label.grid(row=2, column=1, sticky=W)
        self.edit_file_entry = Entry(master)
        self.edit_file_entry.grid(row=2, column=2, sticky='ew')
        # Edit File button
        self.edit_button = Button(master, text="Edit File", command=self.edit_file)
        self.edit_button.grid(row=2, column=0, padx=5, pady=5, sticky='ew')

        # List Files button
        self.list_button = Button(master, text="List Files", command=self.list_files)
        self.list_button.grid(row=3, column=0, padx=5, pady=5, sticky='ew')

        # Content label and entry
        self.edit_content_label = Label(master, text="Content:")
        self.edit_content_label.grid(row=4, column=0, padx=5, sticky=W)
        #self.edit_content_entry = scrolledtext.ScrolledText(master, height=5)  # Set the initial height
        #self.edit_content_entry.grid(row=4, column=1, columnspan=2, padx=5, pady=5, sticky='nsew')
        self.edit_content_entry = Text(master, height=5)
        self.edit_content_entry.grid(row=4, column=1, columnspan=2, padx=5, pady=5, sticky='nsew')
        self.edit_content_entry.bind('<<Modified>>', self.on_content_change)

        # Response area, make it smaller
        self.response_area = scrolledtext.ScrolledText(master, height=8)  # Set the initial height smaller
        self.response_area.grid(row=5, column=0, columnspan=3, padx=5, pady=5, sticky='nsew')

        # Adjust row heights
        master.grid_rowconfigure(4, weight=2)  # Give more weight to the content field row
        master.grid_rowconfigure(5, weight=1)  # Give less weight to the logs field row

    # ... Rest
    def connect(self):
        # Update host with user input
        self.host = self.server_address_entry.get()
        try:
            self.clientSocket.connect((self.host, self.port))
            self.response_area.insert(INSERT, f"Connected to server at {self.host}:{self.port}\n")
            threading.Thread(target=self.receive_messages, daemon=True).start()
        except Exception as e:
            self.response_area.insert(INSERT, f"Failed to connect to server: {e}\n")

    def send_command(self, command):
        try:
            self.clientSocket.sendall(command.encode())
        except Exception as e:
            self.response_area.insert(INSERT, f"Failed to send command: {e}\n")

    def receive_messages(self):
        while True:
            try:
                message = self.clientSocket.recv(1024).decode()
                if message:
                    if message.startswith("Content:") or message.startswith("Update: "):
                        _, filename, updated_content = message.split(' ', 2)
                        # Retrieve the current content of the widget for comparison
                        current_content = self.edit_content_entry.get("1.0", "end-1c").strip()

                        if self.edit_file_entry.get() == filename and current_content != updated_content.strip():
                            self.is_updating_from_server = True  # Disable sending updates
                            print(updated_content)
                            self.edit_content_entry.delete("1.0", END)
                            self.edit_content_entry.insert("1.0", updated_content)
                            self.is_updating_from_server = False  # Re-enable sending updates
                            self.response_area.insert(INSERT, f"Update received for {filename}\n")
                    else:
                        self.response_area.insert(INSERT, f"Server: {message}\n")
                else:
                    break
            except Exception as e:
                self.response_area.insert(INSERT, f"Error receiving message: {e}\n")
                break


    def create_file(self):
        filename = self.create_file_entry.get()
        self.send_command(f"create {filename}")



    #def edit_file(self):
    #    filename = self.edit_file_entry.get()
    #    # Request the current content from the server before editing
    #    self.send_command(f"request_content {filename}")
    #    # Continue with the rest of the method...


    def edit_file(self):
        filename = self.edit_file_entry.get()
        self.send_command(f"get_content {filename}")
        # The actual sending of the updated content is now handled by the on_content_change method.


    def on_content_change(self, event=None):
        print("CHANGING")
        print((self.edit_content_entry.edit_modified()))
        #self.edit_content_entry.edit_modified(True)
        # First, check if the update is from the server to avoid reacting to those changes.
        if self.is_updating_from_server:
            self.edit_content_entry.edit_modified(False)
            return
        if self.edit_content_entry.edit_modified()==1:
            # Get the current content from the text widget.
            current_content = self.edit_content_entry.get("1.0", "end-1c").strip()

            # Assume server_content is the latest content you've received from the server for the file.
            # This would typically be stored as an instance variable updated when server messages are received.
            server_content = self.latest_server_content.strip() if hasattr(self, 'latest_server_content') else ""

            # Compare the current content with the server's content.
            if current_content != server_content:
                # If the content has changed (and it's not just a server update being reflected),
                # send the updated content to the server.
                filename = self.edit_file_entry.get().strip()
                if filename:  # Ensure there's a filename specified.
                    self.send_command(f"edit {filename} {current_content}")

            # Reset the 'modified' state to detect further changes.
            self.edit_content_entry.edit_modified(False)
        



    def list_files(self):
        self.send_command("list")

    def start_sending_updates(self):
        self.update_interval = 0.1  # seconds
        def send_updates():
            while True:
                time.sleep(self.update_interval)
                self.send_file_content()

        # Start the background thread for sending updates
        threading.Thread(target=send_updates, daemon=True).start()

    def send_file_content(self):
        filename = self.edit_file_entry.get()
        content = self.edit_content_entry.get("1.0", END).strip()
        self.send_command(f"edit {filename} {content}")


    #def on_content_change(self, event=None):
    #    content = self.edit_content_entry.get("1.0", END).strip()
    #    filename = self.edit_file_entry.get()
#
    #    if self.edit_content_entry.edit_modified():  # Checks if text widget is modified
    #        self.send_command(f"edit {filename} {content}")
    #        self.edit_content_entry.edit_modified(False)  # Resets the modified flag




if __name__ == "__main__":
    root = Tk()
    root.geometry("600x400")  # Set the initial size of the window
    my_gui = ClientGUI(root)
    #my_gui.start_sending_updates()  # Start sending updates after the GUI is initialized
    root.mainloop()
