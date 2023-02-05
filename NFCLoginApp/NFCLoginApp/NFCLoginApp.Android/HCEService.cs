using Android.App;
using Android.Content;
using Android.Nfc.CardEmulators;
using Android.OS;
using Android.Runtime;
using Android.Util;
using Android.Views;
using Android.Widget;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Text;
using System.Threading.Tasks;

namespace NFCLoginApp.Droid
{
	[
		Service(Exported = true, Enabled = true, Permission = "android.permission.BIND_NFC_SERVICE"),
		IntentFilter(new[] {"android.nfc.cardemulation.action.HOST_APDU_SERVICE"}, Categories = new[] {"android.intent.category.DEFAULT"}),
		MetaData("android.nfc.cardemulation.host_apdu_service", Resource = "@xml/aid_list")
	]
	public class HCEService : HostApduService
	{
		private const string AID = "F06D656F77";

		// ISO-DEP command HEADER for selecting an AID.
		// Format: [Class | Instruction | Parameter 1 | Parameter 2]
		private const string SELECT_APDU_HEADER = "00A40400";

		// "OK" status word sent in response to SELECT AID command (0x9000)
		private static readonly byte[] SELECT_OK_SW = HexStringToByteArray("9000");
		private static byte[] FINGERPRINT_PREAMBLE = Encoding.UTF8.GetBytes("fp|");
		private const string AUTH_PREAMBLE = "auth|";

		// "UNKNOWN" status word sent in response to invalid APDU command (0x0000)
		private static readonly byte[] UNKNOWN_CMD_SW = HexStringToByteArray("0000");
		private static readonly byte[] SELECT_APDU = BuildSelectApdu(AID);
		private static readonly byte[] FINGERPRINT = Encoding.UTF8.GetBytes(Globals.cryptoServices.Fingerprint);

		Context context = Android.App.Application.Context;

		Device currentDevice;

		Func<byte[], byte[]>[] handlers;
		InternalState state;

		enum InternalState
		{
			INIT,
			LOGIN_FP_SENT,
			LOGIN_DATA_WAITING,
			REGISTER_INFO_SENT,
			DEVICE_ADDING
		}

		public HCEService() : base()
		{
			handlers = new Func<byte[], byte[]>[]{
				HandleLogin,
				HandleRegister,
				HandleData
			};

			state = InternalState.INIT;
		}

		public override void OnDeactivated(DeactivationReason reason)
		{
			state = InternalState.INIT;
		}

		public static byte[] BuildSelectApdu(string aid)
		{
			// Format: [CLASS | INSTRUCTION | PARAMETER 1 | PARAMETER 2 | LENGTH | DATA | LE]
			return HexStringToByteArray(SELECT_APDU_HEADER + (aid.Length / 2).ToString("X2") + aid + "00");
		}


		public override byte[] ProcessCommandApdu(byte[] commandApdu, Bundle extras)
		{
			byte[] response;
			return ConcatArrays(new byte[] { 0x01, 0x02, 0x03 }, SELECT_OK_SW);
			foreach (var handler in handlers)
			{
				response = handler(commandApdu);
				if (response != null) return response;
			}

			return UNKNOWN_CMD_SW;
		}

		byte[] HandleLogin(byte[] apdu)
		{
			if (state != InternalState.INIT || !CompareArrays(apdu, SELECT_APDU)) return null;
			state = InternalState.LOGIN_FP_SENT;
			return ConcatArrays(FINGERPRINT_PREAMBLE, FINGERPRINT);
		}

		byte[] HandleRegister(byte[] apdu)
		{
			if (state != InternalState.INIT || !CompareArrays(apdu, SELECT_APDU)) return null;
			state = InternalState.REGISTER_INFO_SENT;
			return ConcatArrays(FINGERPRINT, Encoding.UTF8.GetBytes("|"), Globals.cryptoServices.PublicKeyDER);
		}

		byte[] HandleData(byte[] apdu)
		{
			switch (state)
			{
				case InternalState.INIT:
					return null;

				case InternalState.LOGIN_FP_SENT:
					string[] PCFP = Encoding.UTF8.GetString(apdu).Split('|');
					if (PCFP[0] != "fp") return null;

					currentDevice = Globals.deviceDB.GetDevice(PCFP[1]);
					if (currentDevice == null)
					{
						SendNotification("Device not found");
						state = InternalState.INIT;
						return null;
					}
					else
					{
						state = InternalState.LOGIN_DATA_WAITING;
						return SELECT_OK_SW;
					}

				case InternalState.REGISTER_INFO_SENT:
					string[] PCInfo = Encoding.UTF8.GetString(apdu).Split('|');
					state = InternalState.DEVICE_ADDING;

					AddDevice(PCInfo[0], PCInfo[1]);
					return SELECT_OK_SW;

				case InternalState.LOGIN_DATA_WAITING:
					bool authValid = true;
					for (int i = 0; i < AUTH_PREAMBLE.Length; i++)
					{
						if (apdu[i] != AUTH_PREAMBLE[i])
						{
							authValid = false;
							break;
						}
					}
					if (authValid && currentDevice != null)
					{
						state = InternalState.INIT;
						byte[] response = Globals.cryptoServices.Transcode(currentDevice.PublicKey, apdu[AUTH_PREAMBLE.Length..]);
						currentDevice = null;
						return ConcatArrays(Encoding.UTF8.GetBytes(AUTH_PREAMBLE), response);
					}
					else return new byte[0];

				default:
					return null;
			}
		}

		async void AddDevice(string fp, string pem)
		{
			if (!Globals.IsAppRunning) SendNotification("You have to run the app to register new devices");
			else Globals.deviceDB.AddDevice(fp, pem);
			state = InternalState.INIT;
		}

		void SendNotification(string message)
		{
			var builder = new Notification.Builder(context, MainActivity.CHANNEL_ID)
				.SetAutoCancel(true)
				.SetContentTitle("NFCLoginApp")
				.SetContentText(message);

			var notificationManager = NotificationManager.FromContext(context);
			notificationManager.Notify(MainActivity.NOTIFICATION_ID, builder.Build());
		}

		#region helpers
		public static string ByteArrayToHexString(byte[] bytes)
		{
			string s = "";
			for (int i = 0; i < bytes.Length; i++)
			{
				s += bytes[i].ToString("X2");
			}
			return s;
		}

		private static byte[] HexStringToByteArray(string s)
		{
			int len = s.Length;
			if (len % 2 == 1)
			{
				throw new ArgumentException("Hex string must have even number of characters");
			}
			byte[] data = new byte[len / 2]; //Allocate 1 byte per 2 hex characters
			for (int i = 0; i < len; i += 2)
			{
				ushort val, val2;
				// Convert each chatacter into an unsigned integer (base-16)
				try
				{
					val = (ushort)Convert.ToInt32(s[i].ToString() + "0", 16);
					val2 = (ushort)Convert.ToInt32("0" + s[i + 1].ToString(), 16);
				}
				catch (Exception)
				{
					continue;
				}

				data[i / 2] = (byte)(val + val2);
			}
			return data;
		}

		public static byte[] ConcatArrays(byte[] first, params byte[][] rest)
		{
			int totalLength = first.Length;
			foreach (byte[] array in rest)
			{
				totalLength += array.Length;
			}
			byte[] result = new byte[totalLength];
			first.CopyTo(result, 0);
			int offset = first.Length;
			foreach (byte[] array in rest)
			{
				array.CopyTo(result, offset);
				offset += array.Length;
			}
			return result;
		}

		public static bool CompareArrays(byte[] first, byte[] second)
		{
			if (first.Length != second.Length) return false;

			int len = first.Length;
			for (int i = 0; i < len; i++) if (first[i] != second[i]) return false;

			return true;
		}
		#endregion
	}
}