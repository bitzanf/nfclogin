using Android.App;
using Android.Content;
using Android.OS;
using Android.Provider;
using Android.Runtime;
using Android.Views;
using Android.Widget;
//using Android.Hardware.Biometrics;
using AndroidX.Core.Hardware.Fingerprint;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Xamarin.Forms;
using Java.Security;
using AndroidX.Biometric;
using AndroidX;
using Xamarin.Essentials;
using Java.Lang;
using Java.Interop;

// https://gist.github.com/justintoth/49484bb0c3a0494666442f3e4ea014c0
// https://github.com/nventive/BiometryService/blob/main/src/BiometryService.SampleApp.Uno/BiometryService.SampleApp.Uno.Shared/MainPage.xaml.cs

// https://developer.android.com/reference/android/hardware/biometrics/BiometricPrompt

[assembly: Dependency(typeof(NFCLoginApp.Droid.BioAuth))]
namespace NFCLoginApp.Droid
{
	public class BioAuth : IBioAuth
	{
		Context context = Android.App.Application.Context;

		public void RequestFPAuth(string title, string desc, Action success, Action nosuccess, Action<int, string> error)
		{
			var activity = (MainActivity)Platform.CurrentActivity;

			/*BiometricPrompt prompt = new BiometricPrompt.Builder(context)
				.SetTitle(title)
				.SetDescription(desc)
				.SetDeviceCredentialAllowed(true)
				.SetNegativeButton("Cancel", activity.MainExecutor, DialogInterface.ButtonNegative)
				.Build();*/
			BiometricPrompt prompt = new BiometricPrompt(activity, activity.MainExecutor, GetAuthenticationCallback(success, nosuccess, error));

			//CancellationSignal cancelSignal = new CancellationSignal();
			//var authCallback = GetAuthenticationCallback();
			var promptInfo = new BiometricPrompt.PromptInfo.Builder()
				.SetNegativeButtonText("Cancel")
				.SetTitle(title)
				.SetDescription(desc)
				.SetAllowedAuthenticators(BiometricManager.Authenticators.BiometricStrong)
				.Build();

			prompt.Authenticate(promptInfo);
		}

		public bool CheckFPHW(out string reason)
		{
			BiometricManager biometricManager = BiometricManager.From(context);
			int res = biometricManager.CanAuthenticate(BiometricManager.Authenticators.BiometricStrong);
			switch (res)
			{
				case BiometricManager.BiometricSuccess:
					reason = null;
					return true;
				case BiometricManager.BiometricErrorNoHardware:
					reason = "no hardware";
					return false;
				case BiometricManager.BiometricErrorUnsupported:
					reason = "unsupported";
					return false;
				case BiometricManager.BiometricErrorNoneEnrolled:
					reason = "none enrolled";
					return false;
				default:
					reason = res.ToString();
					return false;
			}
		}

		BiometricPrompt.AuthenticationCallback GetAuthenticationCallback(Action success, Action nosuccess, Action<int, string> error)
		{
			var callback = new BiometricAuthenticationCallback {
				Success = (BiometricPrompt.AuthenticationResult result) => success(),
				Failed = () => nosuccess(),
				//Help = (BiometricAcquiredStatus helpCode, ICharSequence helpString) => {},
				/*Error = (int ec, ICharSequence es) => {
					error(
						(
						ec, System.Enum.GetName(
								typeof(Android.Hardware.Biometrics.BiometricErrorCode),
								ec
							)
						),
						es.ToString()
					);
				}*/
				Error = (int ec, ICharSequence es) => error(ec, es.ToString())
			};

			return callback;
		}
	}

	class BiometricAuthenticationCallback : BiometricPrompt.AuthenticationCallback
	{
		public Action<BiometricPrompt.AuthenticationResult> Success;
		public Action Failed;
		//public Action<BiometricAcquiredStatus, ICharSequence> Help;
		public Action<int, ICharSequence> Error;

		public override void OnAuthenticationSucceeded(BiometricPrompt.AuthenticationResult result)
		{
			base.OnAuthenticationSucceeded(result);
			Success(result);
		}

		public override void OnAuthenticationFailed()
		{
			base.OnAuthenticationFailed();
			Failed();
		}

		public override void OnAuthenticationError(int errorCode, ICharSequence errString)
		{
			base.OnAuthenticationError(errorCode, errString);
			Error(errorCode, errString);
		}

		/*public override void OnAuthenticationHelp([GeneratedEnum] BiometricAcquiredStatus helpCode, ICharSequence helpString)
		{
			base.OnAuthenticationHelp(helpCode, helpString);
			Help(helpCode, helpString);
		}*/
	}
}