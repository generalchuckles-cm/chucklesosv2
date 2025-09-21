import os
import struct
import tkinter as tk
from tkinter import ttk, messagebox, simpledialog, scrolledtext, filedialog

# --- Constants mirroring the C code's hdd_fs.h ---
HDD_SECTOR_SIZE = 512
MAX_FILENAME_LEN = 32
MAX_FILES = 12
MAX_FILE_SIZE = 2 * 1024 * 1024  # 2 MB

# The filesystem starts after a 15MB offset
FS_LBA_OFFSET = 30720  # (15 * 1024 * 1024) / 512

class SimpleFS:
    def __init__(self, disk_path):
        self.disk_path = disk_path
        self.fit = []
        self.next_free_lba = 1 # Relative to the partition start
        self.entry_struct = struct.Struct(f'<{MAX_FILENAME_LEN}sII')

        if not os.path.exists(self.disk_path):
            self.format_disk()
        else:
            self._load_fit()
            self._calculate_next_free_lba()

    def _get_partition_offset(self):
        return FS_LBA_OFFSET * HDD_SECTOR_SIZE

    def _load_fit(self):
        try:
            with open(self.disk_path, "rb") as f:
                f.seek(self._get_partition_offset()) # Seek to partition start
                fit_data = f.read(HDD_SECTOR_SIZE)
            
            self.fit = []
            for i in range(MAX_FILES):
                entry_data = fit_data[i*self.entry_struct.size : (i+1)*self.entry_struct.size]
                if len(entry_data) < self.entry_struct.size: break
                
                filename_bytes, start_lba, size_bytes = self.entry_struct.unpack(entry_data)
                filename = filename_bytes.split(b'\0', 1)[0].decode('utf-8', 'ignore')
                self.fit.append({"filename": filename, "start_lba": start_lba, "size_bytes": size_bytes})
        except Exception as e:
            raise IOError(f"Error loading FIT: {e}")

    def _write_fit(self):
        fit_data = bytearray(HDD_SECTOR_SIZE)
        for i, entry in enumerate(self.fit):
            filename_bytes = entry['filename'].encode('utf-8')
            packed_entry = self.entry_struct.pack(filename_bytes, entry['start_lba'], entry['size_bytes'])
            start, end = i * self.entry_struct.size, (i+1) * self.entry_struct.size
            fit_data[start:end] = packed_entry
        
        with open(self.disk_path, "r+b") as f:
            f.seek(self._get_partition_offset()) # Seek to partition start
            f.write(fit_data)

    def _calculate_next_free_lba(self):
        self.next_free_lba = 1
        for entry in self.fit:
            if entry['filename']:
                num_sectors = (entry['size_bytes'] + HDD_SECTOR_SIZE - 1) // HDD_SECTOR_SIZE
                file_end_lba = entry['start_lba'] + num_sectors
                if file_end_lba > self.next_free_lba:
                    self.next_free_lba = file_end_lba

    def format_disk(self):
        self.fit = [{"filename": "", "start_lba": 0, "size_bytes": 0} for _ in range(MAX_FILES)]
        self.next_free_lba = 1
        self._write_fit()
    
    def list_files(self):
        return [entry for entry in self.fit if entry['filename']]
        
    def read_file(self, filename):
        for entry in self.fit:
            if entry['filename'] == filename:
                if entry['size_bytes'] == 0: return b''
                num_sectors = (entry['size_bytes'] + HDD_SECTOR_SIZE - 1) // HDD_SECTOR_SIZE
                offset = self._get_partition_offset() + (entry['start_lba'] * HDD_SECTOR_SIZE)
                with open(self.disk_path, "rb") as f:
                    f.seek(offset)
                    data = f.read(num_sectors * HDD_SECTOR_SIZE)
                return data[:entry['size_bytes']]
        raise FileNotFoundError(f"File '{filename}' not found.")

    def write_file(self, filename, data: bytes):
        if len(filename.encode('utf-8')) >= MAX_FILENAME_LEN: raise ValueError("Filename too long.")
        if len(data) > MAX_FILE_SIZE: raise ValueError("File data too large.")
        if any(e['filename'] == filename for e in self.fit): raise FileExistsError("File already exists.")
        
        free_index = next((i for i, e in enumerate(self.fit) if not e['filename']), -1)
        if free_index == -1: raise IOError("File table is full.")

        num_sectors = (len(data) + HDD_SECTOR_SIZE - 1) // HDD_SECTOR_SIZE
        file_lba = self.next_free_lba
        
        abs_offset = self._get_partition_offset() + (file_lba * HDD_SECTOR_SIZE)
        with open(self.disk_path, "r+b") as f:
            f.seek(abs_offset)
            f.write(data)

        self.fit[free_index] = {"filename": filename, "start_lba": file_lba, "size_bytes": len(data)}
        self.next_free_lba += num_sectors
        self._write_fit()

    def create_directory(self, dirname):
        if not dirname.endswith('/'):
            dirname += '/'
        self.write_file(dirname, b'')

    def delete_file(self, filename):
        for i, entry in enumerate(self.fit):
            if entry['filename'] == filename:
                self.fit[i] = {"filename": "", "start_lba": 0, "size_bytes": 0}
                self._write_fit()
                return
        raise FileNotFoundError(f"File '{filename}' not found.")

class FileSystemApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.fs = None
        self.file_menu = None
        self.title("Simple FS Tool")
        self.geometry("800x600")
        self.create_menu()
        self.grid_rowconfigure(0, weight=1)
        self.grid_columnconfigure(1, weight=1)
        self.create_widgets()
        self.status_var = tk.StringVar(value="No disk image loaded.")
        ttk.Label(self, textvariable=self.status_var, relief=tk.SUNKEN, anchor="w").grid(row=1, column=0, columnspan=2, sticky="ew")
        self.update_ui_state(is_loaded=False)

    def create_menu(self):
        menubar = tk.Menu(self)
        self.file_menu = tk.Menu(menubar, tearoff=0)
        self.file_menu.add_command(label="Open Disk Image...", command=self.open_disk_image)
        self.file_menu.add_command(label="Close Disk Image", command=self.close_disk_image)
        self.file_menu.add_separator()
        self.file_menu.add_command(label="Exit", command=self.quit)
        menubar.add_cascade(label="File", menu=self.file_menu)
        self.config(menu=menubar)

    def create_widgets(self):
        left_frame = ttk.Frame(self, padding="10")
        left_frame.grid(row=0, column=0, sticky="ns")
        ttk.Label(left_frame, text="Files on Disk", font=("Segoe UI", 10, "bold")).pack(pady=(0,5))
        self.file_listbox = tk.Listbox(left_frame, selectmode=tk.SINGLE, exportselection=False)
        self.file_listbox.pack(expand=True, fill=tk.BOTH)
        self.file_listbox.bind("<<ListboxSelect>>", self.on_file_select)

        button_frame = ttk.Frame(left_frame)
        button_frame.pack(fill=tk.X, pady=10)
        self.btn_add = ttk.Button(button_frame, text="Add File from Host...", command=self.add_file_from_host)
        self.btn_add.pack(fill=tk.X, pady=2)
        self.btn_mkdir = ttk.Button(button_frame, text="New Directory...", command=self.create_directory)
        self.btn_mkdir.pack(fill=tk.X, pady=2)
        self.btn_delete = ttk.Button(button_frame, text="Delete Selected", command=self.delete_selected_file)
        self.btn_delete.pack(fill=tk.X, pady=2)
        self.btn_format = ttk.Button(button_frame, text="Format Partition", command=self.format_disk)
        self.btn_format.pack(fill=tk.X, pady=(10,2))

        right_frame = ttk.Frame(self, padding="10")
        right_frame.grid(row=0, column=1, sticky="nsew")
        right_frame.grid_rowconfigure(1, weight=1)
        right_frame.grid_columnconfigure(0, weight=1)
        ttk.Label(right_frame, text="Filename:").grid(row=0, column=0, sticky="w")
        self.filename_entry = ttk.Entry(right_frame)
        self.filename_entry.grid(row=0, column=0, sticky="ew", padx=(60,0), pady=5)
        self.btn_save = ttk.Button(right_frame, text="Save/Edit File", command=self.save_file)
        self.btn_save.grid(row=0, column=1, padx=10)
        self.content_text = scrolledtext.ScrolledText(right_frame, wrap=tk.WORD, height=10, width=50)
        self.content_text.grid(row=1, column=0, columnspan=2, sticky="nsew")

    def update_ui_state(self, is_loaded):
        state = tk.NORMAL if is_loaded else tk.DISABLED
        self.file_listbox.config(state=state)
        self.btn_add.config(state=state)
        self.btn_mkdir.config(state=state)
        self.btn_delete.config(state=state)
        self.btn_format.config(state=state)
        self.filename_entry.config(state=state)
        self.btn_save.config(state=state)
        self.content_text.config(state=state)
        if self.file_menu:
            self.file_menu.entryconfig("Close Disk Image", state=state)

        if not is_loaded:
            self.file_listbox.delete(0, tk.END)
            self.filename_entry.delete(0, tk.END)
            self.content_text.delete("1.0", tk.END)
            self.title("Simple FS Tool")
            self.status_var.set("No disk image loaded. Use File -> Open.")
        else:
            self.title(f"Simple FS Tool - [{os.path.basename(self.fs.disk_path)}]")
            self.status_var.set(f"Loaded: {self.fs.disk_path}")

    def add_file_from_host(self):
        if not self.fs: return
        host_path = filedialog.askopenfilename(title="Select File to Add", filetypes=[("All Files", "*.*")])
        if not host_path: return
        
        filename = simpledialog.askstring("Enter Filename", "Enter the destination filename on the disk image:", initialvalue=os.path.basename(host_path))
        if not filename: return

        try:
            with open(host_path, "rb") as f:
                content = f.read()
            self.fs.write_file(filename, content)
            messagebox.showinfo("Success", f"File '{filename}' added to disk image.")
        except Exception as e:
            messagebox.showerror("Add File Error", f"Could not add file:\n{e}")
        finally:
            self.refresh_file_list()

    def create_directory(self):
        if not self.fs: return
        dirname = simpledialog.askstring("New Directory", "Enter directory name (e.g., /bin/):")
        if dirname:
            try:
                self.fs.create_directory(dirname)
            except Exception as e:
                messagebox.showerror("Error", f"Could not create directory:\n{e}")
            finally:
                self.refresh_file_list()

    def open_disk_image(self):
        path = filedialog.askopenfilename(title="Open Disk Image", filetypes=[("Disk Image", "*.img"), ("All Files", "*.*")])
        if not path: return
        try:
            self.fs = SimpleFS(path)
            self.update_ui_state(is_loaded=True)
            self.refresh_file_list()
        except Exception as e:
            messagebox.showerror("Error", f"Failed to open disk image:\n{e}")
            self.close_disk_image()

    def close_disk_image(self):
        self.fs = None
        self.update_ui_state(is_loaded=False)

    def refresh_file_list(self):
        if not self.fs: return
        self.file_listbox.delete(0, tk.END)
        try:
            files = self.fs.list_files()
            for info in sorted(files, key=lambda x: x['filename']):
                fname = info['filename']
                if fname.endswith('/'):
                    display_name = f"[DIR] {fname}"
                else:
                    display_name = f"{fname} ({info['size_bytes']} B)"
                self.file_listbox.insert(tk.END, display_name)
        except Exception as e:
            messagebox.showerror("Error", f"Failed to list files: {e}")

    def on_file_select(self, event):
        if not self.fs or not self.file_listbox.curselection(): return
        display_name = self.file_listbox.get(self.file_listbox.curselection()[0])
        
        self.content_text.config(state=tk.NORMAL)
        if display_name.startswith("[DIR]"):
            filename = display_name.split(' ', 1)[1]
            self.filename_entry.delete(0, tk.END)
            self.filename_entry.insert(0, filename)
            self.content_text.delete("1.0", tk.END)
            self.content_text.config(state=tk.DISABLED)
            return

        filename = display_name.split(' (')[0]
        try:
            content = self.fs.read_file(filename).decode('utf-8', 'replace')
            self.filename_entry.delete(0, tk.END)
            self.filename_entry.insert(0, filename)
            self.content_text.delete("1.0", tk.END)
            self.content_text.insert("1.0", content)
        except Exception as e:
            messagebox.showerror("Read Error", f"Could not read file '{filename}':\n{e}")

    def save_file(self):
        if not self.fs: return
        filename = self.filename_entry.get().strip()
        if not filename or filename.endswith('/'):
            messagebox.showwarning("Input Error", "Filename cannot be empty or a directory.")
            return
        content = self.content_text.get("1.0", tk.END).encode('utf-8')

        try:
            try: self.fs.delete_file(filename)
            except FileNotFoundError: pass
            self.fs.write_file(filename, content)
            messagebox.showinfo("Success", f"File '{filename}' saved successfully.")
        except Exception as e:
            messagebox.showerror("Write Error", f"Could not save file:\n{e}")
        finally:
            self.refresh_file_list()

    def delete_selected_file(self):
        if not self.fs or not self.file_listbox.curselection():
            messagebox.showwarning("Selection Error", "Please select a file to delete.")
            return

        display_name = self.file_listbox.get(self.file_listbox.curselection()[0])
        if display_name.startswith("[DIR]"):
            filename = display_name.split(' ', 1)[1]
        else:
            filename = display_name.split(' (')[0]

        if messagebox.askyesno("Confirm Delete", f"Are you sure you want to delete '{filename}'?"):
            try:
                self.fs.delete_file(filename)
                self.filename_entry.delete(0, tk.END)
                self.content_text.delete("1.0", tk.END)
            except Exception as e:
                messagebox.showerror("Delete Error", f"Could not delete file:\n{e}")
            finally:
                self.refresh_file_list()

    def format_disk(self):
        if not self.fs: return
        if messagebox.askyesno("Confirm Format", "WARNING: This will erase all data on the data partition. Are you sure?"):
            try:
                self.fs.format_disk()
                self.filename_entry.delete(0, tk.END)
                self.content_text.delete("1.0", tk.END)
                messagebox.showinfo("Success", "Data partition has been formatted.")
            except Exception as e:
                messagebox.showerror("Format Error", f"Could not format partition:\n{e}")
            finally:
                self.refresh_file_list()

if __name__ == "__main__":
    app = FileSystemApp()
    app.mainloop()