# MSBuild
Playing with the capabilities of MSBuild

	<Target Name="BeforeBuild">
	  <ItemGroup>
	     <AssemblyAttributes Include="AssemblyVersion">
	       <_Parameter1>123.132.123.123</_Parameter1>
	     </AssemblyAttributes>
	  </ItemGroup>
	  <WriteCodeFragment Language="C#" OutputFile="BuildVersion.cs" AssemblyAttributes="@(AssemblyAttributes)" />
	</Target>

	<Content Include="..\..\JSNLog\Scripts\jsnlog.js">
	  <Link>Scripts\jsnlog\jsnlog.js</Link>
	</Content>
	<Target Name="CopyLinkedContentFiles" BeforeTargets="Build">
	    <Copy SourceFiles="%(Content.Identity)" 
	          DestinationFiles="%(Content.Link)" 
	          SkipUnchangedFiles='true' 
	          OverwriteReadOnlyFiles='true' 
	          Condition="'%(Content.Link)' != ''" />
	 </Target>

    <ReadLinesFromFile File="$(LastBuildState)">
      <Output TaskParameter="Lines" ItemName="_ReadProjectStateLine" />
    </ReadLinesFromFile>
	<WriteLinesToFile Overwrite="true" File="$(LastBuildState)" Lines="$(ProjectStateLine);$(ProjectEvaluationFingerprint)"/>
	<Touch AlwaysCreate="true" Files="$(LastBuildUnsuccessful)"/>
	<Delete Files="$(LastBuildUnsuccessful)" Condition="Exists($(LastBuildUnsuccessful))"/>
	<MakeDir Directories="D:\Dogs.txt"/>

	Condition="'$(BuildingInsideVisualStudio)' == 'true'"

	<Project InitialTargets="InvalidPlatformError" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
	    <UsingTask TaskName="VCMessage"
		 AssemblyFile="$(MSBuildThisFileDirectory)Microsoft.Build.CppTasks.Common.dll" />
	
	    <Target Name="InvalidPlatformError">
	      <VCMessage Code="MSB8012" Type="Warning" Arguments="TargetPath;$(TargetPath);Linker;;Link"/>
	    </Target>
	</Project>

[MSBuild Task Reference](https://msdn.microsoft.com/en-us/library/7z253716.aspx):

- Exec
- FindUnderPath
- GetFrameworkSdkPath: Retrieves the path to the Windows Software Development Kit (SDK).
- Move
- MSBuild: Builds MSBuild projects from another MSBuild project.
- RemoveDir
- **RemoveDuplicates**: Removes duplicate items from the specified item collection.
- SetEnv
- Cl

- [Formater la sorties des erreurs / warnings](http://blogs.msdn.com/b/msbuild/archive/2006/11/03/msbuild-visual-studio-aware-error-messages-and-message-formats.aspx)
- [Common MSBuild Project Properties](https://msdn.microsoft.com/en-us/library/bb629394.aspx)
- [How to: Extend the Visual Studio Build Process](https://msdn.microsoft.com/en-us/library/ms366724.aspx) => all of the targets in Microsoft.Common.targets that you can safely override.
 

