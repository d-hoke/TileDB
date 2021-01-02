rem cmake --build . --config RelWithDebInfo  --verbose -- /verbosity:diagnostic /fl 2>&1 > ep_azuresdk.out.3.txt
rem apparently cmake in 2017 didn't know '--verbose'
cmake --build . --config RelWithDebInfo  -- /verbosity:diagnostic /fl 2>&1 | "c:\Program Files\Git\usr\bin\tee.exe" build.out.3.txt
