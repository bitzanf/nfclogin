using System;
using System.Collections.Generic;
using System.Text;
using SQLite;
using System.Security.Cryptography;
using System.IO;
using PemUtils;
using Xamarin.Forms;

namespace NFCLoginApp
{
	[Table("Devices")]
	public class DeviceDBField {
		[PrimaryKey, Column("Fingerprint")]
		public string Fingerprint { get; set; }

		[Column("Name")]
		public string Name { get; set; }

		[Column("Registered")]
		public long Registered { get; set; }

		[Column("PublicKeyPEM")]
		public string PublicKeyPEM { get; set; }
	}

	public class Device
	{
		public string Fingerprint { get; set; }
		public string Name { get; set; }
		public DateTime Registered { get; set; }
		public RSA PublicKey { get; set; }

		public Device(DeviceDBField ddbf)
		{
			Fingerprint = ddbf.Fingerprint;
			Name = ddbf.Name;
			Registered = new DateTime(1970, 1, 1, 0, 0, 0).AddSeconds(ddbf.Registered).ToLocalTime();
			PublicKey = DependencyService.Get<ICryptoServices>().KeyFromPEM(ddbf.PublicKeyPEM);
		}
	}
}
