using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Security.Cryptography;
using System.Text;
using NFCLoginApp;
using PemUtils;
using SQLite;
using Xamarin.Forms.Internals;

namespace NFCLoginApp
{
	public class DeviceDB : INotifyPropertyChanged
	{
		static SQLiteConnection db;
		public string NewDeviceName;

		public DeviceDB()
		{
			if (db == null)
			{
				string PrivatePath = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
				db = new SQLiteConnection(System.IO.Path.Combine(PrivatePath, "devices.sqlite3"));
				db.CreateTable<DeviceDBField>();
			}
		}

		public Device GetDevice(string fp)
		{
			List<DeviceDBField> ddb = db.Query<DeviceDBField>("SELECT * FROM Devices WHERE Fingerprint = ? LIMIT 1", fp);
			return ddb.Count > 0 ? new Device(ddb[0]) : null;
		}

		public void AddDevice(string fp, string pk)
		{
			try
			{
				if (NewDeviceName == null) throw new ArgumentNullException("The new device name cannot be null");

				DeviceDBField device = new DeviceDBField() {
					Name = NewDeviceName,
					Fingerprint = fp,
					PublicKeyPEM = pk,
					Registered = (long)DateTime.Now.ToUniversalTime().Subtract(new DateTime(1970, 1, 1, 0, 0, 0)).TotalSeconds
				};
				db.Insert(device);
				fireDBEvents();
				NewDeviceName = null;
				OnDeviceAdded.Invoke();
			} catch (Exception ex)
			{
				OnDBError.Invoke(ex);
			}
		}

		public void AddDevice(string fp, RSA pk)
		{
			using (var stream = new MemoryStream())
			{
				using (var writer = new PemWriter(stream))
				{
					writer.WritePublicKey(pk);
					AddDevice(fp, stream.ToString());
				}
			}

		}

		public void RemoveDevice(string fp)
		{
			db.Delete<DeviceDBField>(fp);
			fireDBEvents();
		}

		void fireDBEvents()
		{
			if (PropertyChanged != null)
			{
				var properties = GetType().GetProperties();
				properties = properties.Where(p => p.Name.StartsWith("DB_")).ToArray();
				foreach (var p in properties)
				{
					PropertyChanged.Invoke(this, new PropertyChangedEventArgs(p.Name));
				}

			}
		}
		
		public ObservableCollection<Device> DB_Devices => new ObservableCollection<Device>(getAllDevices().Select(ddbf => new Device(ddbf)));
		public int DB_DeviceCount => getAllDevices().Count;

		List<DeviceDBField> getAllDevices() => db.Query<DeviceDBField>("SELECT * FROM Devices");


		#region INotifyPropertyChanged
		protected bool SetProperty<T>(
			ref T backingStore,
			T value,
			[CallerMemberName] string propertyName = "",
			Action onChanged = null
		)
		{
			if (EqualityComparer<T>.Default.Equals(backingStore, value))
				return false;

			backingStore = value;
			onChanged?.Invoke();
			OnPropertyChanged(propertyName);
			return true;
		}

		public delegate void DeviceAddedHandler();
		public event DeviceAddedHandler OnDeviceAdded;

		public delegate void DBErrorHandler(Exception ex);
		public event DBErrorHandler OnDBError;

		public event PropertyChangedEventHandler PropertyChanged;
		protected void OnPropertyChanged([CallerMemberName] string propertyName = "")
		{
			PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
		}
		#endregion
	}
}
