ThisBuild / scalaVersion := "3.7.2"
ThisBuild / organization := "org.example"

lazy val root = (project in file("."))
  .settings(
    name := "elf-lang",
    version := "0.1.0-SNAPSHOT",

    libraryDependencies ++= Seq(
      "org.scala-lang.modules" %% "scala-parser-combinators" % "2.4.0",
      "com.lihaoyi" %% "ujson" % "4.1.0",
      "org.scalatest" %% "scalatest" % "3.2.18" % Test
    ),

    assembly / mainClass := Some("elflang.Main"),
    assembly / assemblyJarName := "elf-lang-assembly-0.1.0.jar",

    // Merge strategy for assembly
    assembly / assemblyMergeStrategy := {
      case "module-info.class" => MergeStrategy.discard
      case "META-INF/versions/9/module-info.class" => MergeStrategy.discard
      case x => (assembly / assemblyMergeStrategy).value(x)
    }
  )